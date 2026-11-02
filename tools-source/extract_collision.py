from pathlib import Path
import struct,re,json,math,hashlib,sys
from kv3_text import parse
def extract(source,output):
    source=Path(source);output=Path(output);out=output.open('wb');primitives=output.with_suffix('.rawprim').open('wb');record={};attribute=0;shape='';count=0;shapes=0;primitive_count=0
    def emit(a,b,c):
        nonlocal count
        if not all(math.isfinite(v)and abs(v)<1e8 for p in [a,b,c]for v in p):raise ValueError('Invalid collision vertex')
        out.write(struct.pack('<9fI',*(a+b+c),attribute));count+=1
    def flush():
        nonlocal shapes,primitive_count
        if not record:return
        if shape in ['m_Capsule','m_Sphere']:
            center=record['primitive']['m_vCenter'];radius=record['primitive']['m_flRadius'];a,b=(center,center)if shape=='m_Sphere'else center
            assert radius>0 and len(a)==3 and len(b)==3
            primitives.write(struct.pack('<7fI',*(a+b),radius,attribute));primitive_count+=1;return
        vertices=record.get('m_VertexPositions',record.get('m_Vertices',b''))
        if shape in ['m_Hull','m_Mesh']and vertices:
            if len(vertices)%12:raise ValueError('Invalid position buffer')
            points=list(struct.iter_unpack('<3f',vertices))
            if shape=='m_Mesh':
                triangles=record.get('m_Triangles',b'');assert len(triangles)%12==0
                for a,b,c in struct.iter_unpack('<3I',triangles):emit(points[a],points[b],points[c])
            else:
                edges=record.get('m_Edges',b'');faces=record.get('m_Faces',b'');assert len(edges)%4==0
                for face,first in enumerate(faces):
                    edge=first;polygon=[];seen=set()
                    while edge not in seen:
                        seen.add(edge);nxt,twin,origin,face_index=struct.unpack_from('<4B',edges,edge*4)
                        if face_index!=face or origin>=len(points):raise ValueError('Invalid hull edge topology')
                        polygon.append(origin);edge=nxt
                    if edge!=first or len(polygon)<3:raise ValueError('Open collision hull')
                    for i in range(1,len(polygon)-1):emit(points[polygon[0]],points[polygon[i]],points[polygon[i+1]])
            shapes+=1
    def blob(stream,line):
        text=line[line.index('#[')+2:];data=bytearray()
        while True:
            if ']'in text:data.extend(bytes.fromhex(text[:text.index(']')].strip()));break
            data.extend(bytes.fromhex(text.strip()));text=next(stream)
        return bytes(data)
    attributes=None;depth=0;container=None;container_depth=0;record_depth=0;pending=None
    with source.open(encoding='utf-8-sig')as stream:
        for line in stream:
            stripped=line.strip();old_depth=depth;depth+=line.count('{')-line.count('}')
            if record_depth and depth<record_depth:flush();record={};shape='';record_depth=0
            if stripped.startswith('m_collisionAttributes ='):
                flush();record={};tail=line+''.join(stream);attributes=parse('{\n'+tail)['m_collisionAttributes'];break
            match=re.match(r'\s*m_nCollisionAttributeIndex\s*=\s*(\d+)',line)
            if match:attribute=int(match[1]);continue
            if stripped in ['m_Hull =','m_Mesh =','m_Capsule =','m_Sphere =']:
                pending=stripped.split()[0];continue
            names={'m_Hulls':'m_Hull','m_Meshes':'m_Mesh','m_Capsules':'m_Capsule','m_Spheres':'m_Sphere'}
            key=stripped.split(' =')[0]
            if key in names and '[  ]'not in stripped:container=names[key];container_depth=depth;continue
            if stripped==']'and depth==container_depth:container=None
            if stripped=='{'and(pending or(container and depth==container_depth+1)):
                flush();record={};shape=pending or container;record_depth=depth;pending=None
                if shape in ['m_Capsule','m_Sphere']:
                    body=[line];nested=1
                    for part in stream:
                        body.append(part);nested+=part.count('{')-part.count('}')
                        if nested==0:break
                    record={'primitive':parse(''.join(body).strip().rstrip(','))};flush();record={};shape='';record_depth=0;depth-=1
                continue
            match=re.match(r'\s*(m_VertexPositions|m_Vertices|m_Edges|m_Faces|m_Triangles)\s*=\s*(.*)',line)
            if match and record_depth:
                rest=match[2]
                if '#['not in rest:rest=next(stream)
                if '#['in rest:record[match[1]]=blob(stream,rest)
    out.close();primitives.close();assert attributes is not None;assert count>0
    masks=[]
    for attr in attributes:
        group=str(attr.get('m_CollisionGroupString','')).lower();as_=set(x.lower()for x in attr.get('m_InteractAsStrings',[]));exclude=set(x.lower()for x in attr.get('m_InteractExcludeStrings',[]))
        # Bits: visible line, bullet path, physical projectile path, spell LOS.
        solid='solid'in as_ or(group=='default'and not as_);mask=15 if solid else 0
        if 'blocklos'in as_:mask|=1
        if 'window'in as_:mask|=2|4|8
        if 'citadel_bullet'in exclude:mask&=~2
        if 'citadel_projectile'in exclude:mask&=~4
        if 'citadel_ability'in exclude:mask&=~8
        masks.append(mask)
    metadata=dict(source=str(source.name),source_text_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),triangles=count,shapes=shapes,primitives=primitive_count,attributes=attributes,masks=masks)
    output.with_suffix('.json').write_text(json.dumps(metadata,indent=2));print(output.name,count,'triangles',shapes,'shapes',len(masks),'collision groups')
if __name__=='__main__':extract(sys.argv[1],sys.argv[2])
