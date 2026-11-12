#!/usr/bin/env python3
"""Render actual GeoJSON observation points as a self-contained, north-up SVG score.

No basemap or estimated location fills. Gaps stay gaps. No internet or dependencies.
"""
import argparse, json, math
from pathlib import Path

def render(data, width=1000, height=730):
    features=data.get('features',[])
    rows=[]
    for f in features:
        try:
            lon,lat=f['geometry']['coordinates']
            t=int(f['properties']['recv_ms'])
            rms=int(f['properties']['rms_mg'])
            impacts=int(f['properties']['impacts'])
            ride=int(f['properties']['ride_id'])
        except (KeyError,ValueError,TypeError): continue
        if not(-90<=lat<=90 and -180<=lon<=180): continue
        rows.append((float(lon),float(lat),t,rms,impacts,ride))
    title='TRAIL / ECHO'; ink='#dce6e1'; dim='#90a6a0'; bg='#0a1115'
    out=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" role="img" aria-label="Trail Echo GPS observation score">',
       f'<rect width="{width}" height="{height}" fill="{bg}"/>',
       '<defs><pattern id="grid" width="48" height="48" patternUnits="userSpaceOnUse"><path d="M48 0H0V48" fill="none" stroke="#213035" stroke-width="1"/></pattern></defs>',
       f'<rect x="40" y="115" width="{width-80}" height="{height-170}" fill="url(#grid)"/>',
       f'<text x="44" y="60" fill="{ink}" font-family="Helvetica,Arial" font-size="28" font-weight="bold" letter-spacing="2">{title}</text>',
       f'<text x="44" y="87" fill="{dim}" font-family="monospace" font-size="13">GPS / TERRAIN OBSERVATIONS   ·   N ↑</text>']
    if rows:
        # Equirectangular meters around observed latitude, sufficient for a short local ride.
        mean_lat=sum(p[1] for p in rows)/len(rows)
        scale_lon=math.cos(math.radians(mean_lat))
        lon0=min(p[0] for p in rows); lat0=min(p[1] for p in rows)
        xy=[((p[0]-lon0)*scale_lon,p[1]-lat0) for p in rows]
        xs=[a for a,b in xy];ys=[b for a,b in xy]
        minx,maxx=min(xs),max(xs);miny,maxy=min(ys),max(ys)
        ext=max(maxx-minx,maxy-miny,1e-9)
        s=min((width-170)/ext,(height-255)/ext)
        ox=(width-(maxx-minx)*s)/2; oy=145+((height-270)-(maxy-miny)*s)/2
        pts=[(ox+(x-minx)*s,oy+(maxy-y)*s) for x,y in xy]
        # Only contiguous, same-ride real observations within 2.5s form a segment.
        segments=0
        for i in range(1,len(rows)):
            a,b=rows[i-1],rows[i]
            if b[5]!=a[5] or b[2]<=a[2] or b[2]-a[2]>2500:continue
            x1,y1=pts[i-1];x2,y2=pts[i]
            intensity=b[3];col='#e7834c' if b[4] else '#b4ed70' if intensity>=350 else '#7ea49e'
            w=2+min(5,intensity/350)
            out.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{col}" stroke-width="{w:.1f}" stroke-linecap="round"/>')
            segments+=1
        for (x,y),p in zip(pts,rows):
            if p[4]:out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="5" fill="#e7834c" stroke="{bg}" stroke-width="2"/>')
            elif segments==0:out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.5" fill="#b4ed70"/>')
    else:
        out.append(f'<text x="44" y="170" font-size="20" fill="{dim}" font-family="monospace">NO VALID GPS / IMU MATCHES</text>')
    out += [f'<line x1="44" y1="{height-67}" x2="{width-44}" y2="{height-67}" stroke="#2d4643"/>',
        f'<text x="44" y="{height-41}" fill="#b4ed70" font-size="14" font-family="monospace">● FLOW / LIGHT VIBRATION</text>',
        f'<text x="375" y="{height-41}" fill="#e7834c" font-size="14" font-family="monospace">● IMPACTS</text>',
        f'<text x="{width-270}" y="{height-41}" fill="{dim}" font-size="14" font-family="monospace">{len(rows)} observed points</text>',
        '</svg>']
    return '\n'.join(out)+'\n'

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('geojson',type=Path);a.add_argument('output',type=Path)
    args=a.parse_args()
    args.output.write_text(render(json.loads(args.geojson.read_text())))
    print('Wrote',args.output)
if __name__=='__main__':main()
