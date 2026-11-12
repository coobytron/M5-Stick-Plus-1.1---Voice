#!/usr/bin/env python3
"""Join on Cardputer-local reception time to make a real, discontinuous GeoJSON trail score."""
import argparse,csv,json,bisect,sys
from pathlib import Path

def readrows(path):
    with open(path,encoding='utf-8-sig',newline='') as f:
        return list(csv.DictReader(f))

def build(gps_rows,imu_rows,max_gap_ms=1800):
    fixes=[]
    invalid_times=[]
    for row in gps_rows:
        if row.get('fix','').strip()!='1':
            try: invalid_times.append(int(row['local_ms']))
            except (KeyError,ValueError,TypeError): pass
        if row['fix'].strip()!='1' or not row.get('lat') or not row.get('lon'):continue
        try:
            t=int(row['local_ms']);lat=float(row['lat']);lon=float(row['lon'])
        except (ValueError,TypeError):continue
        if not(-90<=lat<=90 and -180<=lon<=180):continue
        fixes.append((t,lat,lon,row))
    fixes.sort()
    invalid_times.sort()
    times=[f[0] for f in fixes]
    geo=[];dropped=0;invalid=0
    seen=set()
    for r in imu_rows:
        try:
            t=int(r['rx_ms']);ride=int(r['ride_id']);seq=int(r['seq']);rms=int(r['rms_mg']);impact=int(r['impacts'])
        except (KeyError,ValueError,TypeError):invalid+=1;continue
        key=(ride,seq)
        if key in seen:continue
        seen.add(key)
        if not times:dropped+=1;continue
        idx=bisect.bisect_left(times,t)
        candidates=[fixes[j] for j in (idx-1,idx) if 0<=j<len(fixes)]
        when,lat,lon,fix=min(candidates,key=lambda x:abs(x[0]-t))
        if abs(when-t)>max_gap_ms:dropped+=1;continue
        # A logged NO FIX is a hard boundary: never match across a known outage.
        lo,hi=min(when,t),max(when,t)
        n=bisect.bisect_left(invalid_times,lo)
        if n<len(invalid_times) and invalid_times[n]<=hi:dropped+=1;continue
        geo.append({'type':'Feature','geometry':{'type':'Point','coordinates':[lon,lat]},
            'properties':{'ride_id':ride,'sequence':seq,'rms_mg':rms,'impacts':impact,
            'recv_ms':t,'gps_ms':when,'offset_ms':t-when,'utc':fix.get('utc_iso',''),
            'color': '#e7834c' if impact else '#b4ed70' if rms>=350 else '#8aaba0'}})
    return {'type':'FeatureCollection','properties':{
        'label':'Trail Echo GPS Bridge','joined_points':len(geo),'unmatched_motion':dropped,
        'invalid_motion':invalid,'gps_fixes':len(fixes),'join_tolerance_ms':max_gap_ms,
        'disclaimer':'Sparse points only. No route segment across gaps; no inferred positions.'},
        'features':geo}

def main(argv=None):
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--gps',required=True,type=Path);a.add_argument('--imu',required=True,type=Path)
    a.add_argument('--output',default='trail-echo.geojson',type=Path)
    a.add_argument('--max-gap-ms',default=1800,type=int)
    args=a.parse_args(argv)
    obj=build(readrows(args.gps),readrows(args.imu),args.max_gap_ms)
    args.output.write_text(json.dumps(obj,indent=2)+'\n')
    print(f"Wrote {args.output}: {obj['properties']['joined_points']} real observations, {obj['properties']['unmatched_motion']} missing GPS matches")
if __name__=='__main__':main()
