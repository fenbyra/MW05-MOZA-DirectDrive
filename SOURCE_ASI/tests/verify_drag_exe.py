"""Read-only offline verification of the exact supplied PE32. Never executes the game."""
from pathlib import Path
import hashlib,struct,re,sys
b=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()=='325304a2cdd5d1d5789c09c90d249774743d1b273889b1093339ac922d56a968'
pe=struct.unpack_from('<I',b,0x3c)[0];n=struct.unpack_from('<H',b,pe+6)[0];opt=struct.unpack_from('<H',b,pe+20)[0]
base=struct.unpack_from('<I',b,pe+24+28)[0];assert base==0x400000
sections=[]
for i in range(n):
 at=pe+24+opt+40*i
 _,rva,size,raw=struct.unpack_from('<IIII',b,at+8);sections.append((rva,size,raw))
def read(va,size):
 for rva,n,raw in sections:
  if rva<=va-base and va-base+size<=rva+n:return b[raw+va-base-rva:raw+va-base-rva+size]
 raise AssertionError(hex(va))
def u32(va):return struct.unpack('<I',read(va,4))[0]
s=(Path(__file__).resolve().parents[1]/'examples/mw05_mirror_probe/direction_core.h').read_text()
for name in ['DragCtor','DragFetch']:
 rva=int(re.search(r'k'+name+r'Rva\s*=\s*(0x[\da-fA-F]+)u',s)[1],16)
 a=re.search(r'k'+name+r'\[\]\s*=\s*\{(.*?)\}',s,re.S)[1]
 expected=bytes(int(x,16) for x in re.findall(r'0x([\da-fA-F]+)',a))
 assert read(base+rva,len(expected))==expected,name
for vt in [0x8ac670,0x8ac708]:
 for slot,value in [(2,0x69cbb0),(5,0x69cb80),(6,0x69cb90),(7,0x69cba0)]:
  assert u32(vt+slot*4)==value,(hex(vt),slot)
assert read(0x69cbb0,4)==bytes.fromhex('8d 41 08 c3')
assert read(0x69cb80,10)==bytes.fromhex('8b 44 24 04 89 41 0c c2 04 00')
print('PASS: exact EXE SHA256; drag constructor, base-fetch delegation, both getter/steer/gas/brake tables and common getter/setter code verified offline.')

# Specialized transmission is only a statically verified candidate until DRIVE_DIAG observes it.
d=(Path(__file__).resolve().parents[1]/'examples/mw05_mirror_probe/drive_probe.h').read_text()
rva=int(re.search(r'kDragTransmissionCtorRva\s*=\s*(0x[\da-fA-F]+)u',d)[1],16)
a=re.search(r'kDragTransmissionCtor\[\]\s*=\s*\{(.*?)\}',d,re.S)[1]
expected=bytes(int(x,16) for x in re.findall(r'0x([\da-fA-F]+)',a))
assert read(base+rva,len(expected))==expected
assert u32(0x8abfc8+4)==0x6a0590
assert read(0x6a0590,4)==bytes.fromhex('8b 41 38 c3')
assert read(0x6b3b55,5)==bytes.fromhex('e8 56 fc ff ff')
print('PASS: specialized transmission constructor, base constructor call, shared GetGear and +38 layout verified offline; live transmission table remains to be observed.')
