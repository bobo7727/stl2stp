import math, struct, sys, os
out = sys.argv[1]
def cube(n, off=(0,0,0), skip_top=False):
    res=[]
    for axis in range(3):
        for side in range(2):
            if skip_top and axis==2 and side==1: continue
            u=(axis+1)%3; v=(axis+2)%3
            def at(i,j):
                p=[0,0,0]; p[axis]=side; p[u]=i/n; p[v]=j/n
                return (p[0]+off[0],p[1]+off[1],p[2]+off[2])
            for i in range(n):
                for j in range(n):
                    a=[at(i,j),at(i+1,j),at(i+1,j+1)]; b=[at(i,j),at(i+1,j+1),at(i,j+1)]
                    if side==0: a=[a[0],a[2],a[1]]; b=[b[0],b[2],b[1]]
                    res+= [a,b]
    return res
def rotated(tris):
    ax=[1/math.sqrt(14),2/math.sqrt(14),3/math.sqrt(14)]; c=math.cos(.7); s=math.sin(.7)
    def r(p):
        k=(ax[1]*p[2]-ax[2]*p[1], ax[2]*p[0]-ax[0]*p[2], ax[0]*p[1]-ax[1]*p[0])
        d=sum(ax[i]*p[i] for i in range(3)); t=(12.5,-3.25,40.0)
        return tuple(p[i]*c+k[i]*s+ax[i]*d*(1-c)+t[i] for i in range(3))
    return [[r(p) for p in tri] for tri in tris]
def write(name, tris):
    with open(os.path.join(out,name),'wb') as f:
        f.write(b'cadcheck2'.ljust(80,b' ')); f.write(struct.pack('<I',len(tris)))
        for t in tris:
            f.write(struct.pack('<3f',0,0,0))
            for p in t: f.write(struct.pack('<3f',*p))
            f.write(b'\0\0')
write('rot-cube-4.stl', rotated(cube(4)))
write('rot-cube-10.stl', rotated(cube(10)))
write('open-cube-4.stl', cube(4, skip_top=True))
write('rot-two-bodies.stl', rotated(cube(4)+cube(4,(2,0,0))))
print('ok')
