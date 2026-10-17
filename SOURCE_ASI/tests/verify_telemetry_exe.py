"""Offline read-only exact EXE checks; never executes or edits it."""
import argparse,hashlib,struct,json
from pathlib import Path
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--out',type=Path);a=ap.parse_args()
b=a.exe.read_bytes();digest=hashlib.sha256(b).hexdigest()
assert digest=='325304a2cdd5d1d5789c09c90d249774743d1b273889b1093339ac922d56a968'
pe=struct.unpack_from('<I',b,0x3c)[0];n=struct.unpack_from('<H',b,pe+6)[0];opt=struct.unpack_from('<H',b,pe+20)[0];sec=pe+24+opt
sections=[struct.unpack_from('<IIII',b,sec+i*40+8) for i in range(n)]
def read(va,size):
 rva=va-0x400000
 for vs,vr,rs,raw in sections:
  if vr<=rva and rva+size<=vr+rs:return b[raw+rva-vr:raw+rva-vr+size]
 raise AssertionError(hex(va))
checks={
 0x57a6e0:'83 ec 30 56 8b f1',
 0x57a8b7:'5e 83 c4 30 c2 04 00',0x57a8f2:'5e 83 c4 30 c2 04 00',
 0x8a2260:'c0 eb 59 00 e0 a6 57 00',
 0x8a2248:'b0 eb 59 00 30 eb 59 00 40 eb 59 00',
 0x57a708:'d9 46 54 d8 76 5c',0x57a75b:'d9 46 54 d8 5e 58',
 0x59ea1e:'8d 6e 30',0x59ea41:'c7 06 60 22 8a 00 c7 45 00 48 22 8a 00'
}
for va,data in checks.items():assert read(va,len(bytes.fromhex(data)))==bytes.fromhex(data),hex(va)
result={'sha256':digest,'checks':{f'{x:08X}':y for x,y in checks.items()},'limit':'HUD code/layout verified statically; real hook integration/ownership/physical comparison pending'}
if a.out:a.out.write_text(json.dumps(result,indent=2))
print('PASS: 9 exact-EXE HUD code/layout/vtable checks; Windows/physical integration pending')
