#!/usr/bin/env python3
"""Join on Cardputer-local reception time to make a real, discontinuous GeoJSON trail score.

--sync adds a ride the Stick resent after it was recorded (/TrailEcho/Snn.CSV).
Its seconds are placed on the Cardputer clock using live packets from the same
ride when there are any, or else --start-utc (when the Stick ride started, read
from the GPS log's utc_iso column). Seconds already received live keep their
live timing; the sync only fills the gaps.
"""
import argparse,csv,json,bisect,sys,statistics
from datetime import datetime,timezone
from pathlib import Path

def readrows(path):
    with open(path,encoding='utf-8-sig',newline='') as f:
        return list(csv.DictReader(f))

def _int(v):
    try: return int(v)
    except (ValueError,TypeError): return None

def _utc(text):
    text=(text or '').strip()
    if not text: return None
    try: return datetime.strptime(text,'%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc).timestamp()
    except ValueError: return None

def sync_to_imu(gps_rows,imu_rows,sync_rows,start_utc=None):
    """Turn replayed seconds into extra imu-style rows on the Cardputer clock.
    Returns (rows, info). Never guesses: with no anchor, nothing is placed."""
    info={'sync_seconds':0,'sync_placed':0,'sync_anchor':'none'}
    rides={}
    for r in sync_rows:
        ride=_int(r.get('ride_id'));el=_int(r.get('elapsed_s'))
        if ride is None or el is None or el<1:continue
        rides.setdefault(ride,[]).append((el,r))
        info['sync_seconds']+=1
    out=[]
    for ride,rows in rides.items():
        start=None
        # Anchor 1: live packets of the same ride (only when the Stick knew the ride id).
        if ride:
            offs=[_int(r['rx_ms'])-_int(r['elapsed_s'])*1000 for r in imu_rows
                  if _int(r.get('ride_id'))==ride and _int(r.get('rx_ms')) is not None and _int(r.get('elapsed_s')) is not None]
            if offs:start=statistics.median(offs);info['sync_anchor']='live'
        # Anchor 2: a start time given by the rider, mapped through the GPS log's own clock.
        if start is None and start_utc is not None:
            pairs=[(_utc(g.get('utc_iso')),_int(g.get('local_ms'))) for g in gps_rows]
            offs=[l-u*1000 for u,l in pairs if u is not None and l is not None]
            if offs:start=statistics.median(offs)+start_utc*1000;info['sync_anchor']='start-utc'
        if start is None:continue
        for el,r in rows:
            out.append({'rx_ms':str(int(start+el*1000)),'ride_id':str(ride),'seq':str(el-1),'elapsed_s':str(el),
                        'rms_mg':r.get('rms_mg',''),'peak10mg':r.get('peak10mg',''),'impacts':r.get('impacts',''),
                        'tx_ms':'','source':'sync'})
            info['sync_placed']+=1
    return out,info

def build(gps_rows,imu_rows,max_gap_ms=1800,sync_rows=None,start_utc=None):
    extra,sync_info=sync_to_imu(gps_rows,imu_rows,sync_rows or [],start_utc)
    imu_rows=list(imu_rows)+extra  # live rows first, so they win over a sync copy
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
        # Same ride + same second is one observation, live or resent.
        el=_int(r.get('elapsed_s'))
        key=(ride,'el',el) if el is not None and el>0 else (ride,'seq',seq)
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
            'source':r.get('source','live'),
            'color': '#e7834c' if impact else '#b4ed70' if rms>=350 else '#8aaba0'}})
    return {'type':'FeatureCollection','properties':{
        'label':'Trail Echo GPS Bridge','joined_points':len(geo),'unmatched_motion':dropped,
        'invalid_motion':invalid,'gps_fixes':len(fixes),'join_tolerance_ms':max_gap_ms,
        **sync_info,
        'disclaimer':'Sparse points only. No route segment across gaps; no inferred positions.'},
        'features':geo}

def main(argv=None):
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--gps',required=True,type=Path);a.add_argument('--imu',required=True,type=Path)
    a.add_argument('--sync',type=Path,action='append',default=[],help='Snn.CSV from a ride the Stick resent (repeatable)')
    a.add_argument('--start-utc',help='when the Stick ride started, e.g. 2026-11-13T15:04:05Z; only needed if no live packets came through')
    a.add_argument('--output',default='trail-echo.geojson',type=Path)
    a.add_argument('--max-gap-ms',default=1800,type=int)
    args=a.parse_args(argv)
    start=None
    if args.start_utc:
        start=_utc(args.start_utc)
        if start is None:a.error('--start-utc must look like 2026-11-13T15:04:05Z')
    sync=[row for p in args.sync for row in readrows(p)]
    obj=build(readrows(args.gps),readrows(args.imu),args.max_gap_ms,sync,start)
    args.output.write_text(json.dumps(obj,indent=2)+'\n')
    pr=obj['properties']
    print(f"Wrote {args.output}: {pr['joined_points']} real observations, {pr['unmatched_motion']} missing GPS matches")
    if sync:
        print(f"Sync: {pr['sync_placed']} of {pr['sync_seconds']} resent seconds placed (anchor: {pr['sync_anchor']})")
        if pr['sync_placed']<pr['sync_seconds']:
            print("Some resent seconds had no anchor: no live packets from that ride. Pass --start-utc with the ride's start time.",file=sys.stderr)
if __name__=='__main__':main()
