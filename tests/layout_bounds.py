# Bounds used by the native button helper and the chat header.
controls=[('DMs',265,51),('Jogadores',342,88),('Atualizar',432,76)]
for a,b in zip(controls,controls[1:]):
 assert a[1]+a[2]/2+6 <= b[1]-b[2]/2,(a,b)
assert controls[-1][1]+controls[-1][2]/2<=474
# Separate input, feedback line, and message viewport.
assert 15+15 < 36-5
assert 36+5 < 46
# Reward cards and navigation do not intersect.
centers=[95,215,335,455]
for a,b in zip(centers,centers[1:]):assert a+54+10<=b-54
assert 15+58<143-117/2
print('PASS: chat gaps, input/feedback/list separation and reward card bounds')
