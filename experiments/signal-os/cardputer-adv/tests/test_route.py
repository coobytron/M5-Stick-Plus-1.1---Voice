from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from make_route import build

gps=[{'local_ms':'1000','utc_iso':'2026-11-11T12:00:00Z','fix':'1','lat':'34.0','lon':'-117.0'},
 {'local_ms':'2000','utc_iso':'','fix':'0','lat':'','lon':''},
 {'local_ms':'3000','utc_iso':'2026-11-11T12:00:02Z','fix':'1','lat':'34.1','lon':'-117.1'}]
imu=[{'rx_ms':'1100','ride_id':'2','seq':'0','rms_mg':'120','impacts':'0'},
 {'rx_ms':'2999','ride_id':'2','seq':'1','rms_mg':'700','impacts':'2'},
 {'rx_ms':'2100','ride_id':'2','seq':'3','rms_mg':'800','impacts':'0'},
 {'rx_ms':'10000','ride_id':'2','seq':'2','rms_mg':'400','impacts':'0'},
 {'rx_ms':'1110','ride_id':'2','seq':'0','rms_mg':'120','impacts':'0'}]
r=build(gps,imu,max_gap_ms=500)
assert len(r['features'])==2
assert r['features'][1]['geometry']['coordinates']==[-117.1,34.1]
assert r['properties']['unmatched_motion']==2
assert r['features'][1]['properties']['impacts']==2
print('PASS trail GPS route real fixes and gaps')
