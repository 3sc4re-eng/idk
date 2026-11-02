import re,json
def parse(text):
    tokens=re.findall(r'<!--.*?-->|/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}\[\]=,:]|[^\s{}\[\]=,:]+',text,re.S)
    tokens=[v for v in tokens if not(v.startswith('<!--')or v.startswith('//')or v.startswith('/*'))];index=0
    def value():
        nonlocal index
        t=tokens[index];index+=1
        if t=='{':
            result={}
            while tokens[index]!='}':
                key=tokens[index];index+=1
                if key==',':continue
                if key.startswith('"'):key=json.loads(key)
                assert tokens[index]=='=',(key,tokens[index]);index+=1;result[key]=value()
            index+=1;return result
        if t=='[':
            result=[]
            while tokens[index]!=']':
                if tokens[index]==',':index+=1;continue
                result.append(value())
            index+=1;return result
        if index<len(tokens)and tokens[index]==':':index+=1;return value()
        if t.startswith('"'):return json.loads(t)
        if t=='true':return True
        if t=='false':return False
        if t=='null':return None
        try:return float(t)if any(c in t.lower()for c in ['.','e'])else int(t,0)
        except ValueError:return t
    result=value();assert index==len(tokens),(index,len(tokens));return result
