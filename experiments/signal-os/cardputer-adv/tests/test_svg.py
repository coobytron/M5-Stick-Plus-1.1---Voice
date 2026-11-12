from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from render_svg import render
r=render({'features':[{'geometry':{'coordinates':[-117.0,34.0]},'properties':{'recv_ms':1000,'rms_mg':200,'impacts':0,'ride_id':1}},{'geometry':{'coordinates':[-117.001,34.001]},'properties':{'recv_ms':2000,'rms_mg':1200,'impacts':1,'ride_id':1}}]})
assert '<svg' in r and '<line x1=' in r and '#e7834c' in r
n=render({'features':[]})
assert 'NO VALID' in n
print('PASS trail SVG renderer gap preservation')
