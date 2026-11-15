"""Regression guard for the queue congestion and separate-label placement bugs."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/services/GeoPassService.cpp').read_text()
assert 'if(!quest&&!drop)return' in s
assert '"geobr_activity_v4"' in s and '"geobr_activity"' not in s
assert 'schedule_selector(GeoPassService::drain),this,1.f' in s
assert 'm_queue.begin()+(m_sending?1:0)' in s
h=(r/'src/hooks/RankHook.cpp').read_text()
assert 'm_starsLabel->getPosition()' in h and 'float shift=rank?15.f:0' in h
assert 'nameParts' not in h and 'visibleBottom' not in h
print('PASS: useful event filtering, single RPC, rapid queue drain, dedicated TOP slot')
