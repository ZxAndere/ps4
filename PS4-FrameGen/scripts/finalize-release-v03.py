#!/usr/bin/env python3
"""Complete the release ZIP without replacing any previous archive."""
import hashlib,json,pathlib,zipfile
R=pathlib.Path(__file__).resolve().parents[1];D=R/'artifacts/releases/0.3'
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 old=D/'PS4-FrameGen-0.3-project.zip';target=D/'PS4-FrameGen-0.3-project-final.zip'
 if target.exists():raise RuntimeError('Final ZIP already exists')
 required=json.loads((R/'docs/build-manifest-v03.json').read_text())['files']
 with zipfile.ZipFile(old) as src:
  names=set(src.namelist())
  names.update('PS4-FrameGen/'+n for n in required)
  names.add('PS4-FrameGen/scripts/finalize-release-v03.py')
  with zipfile.ZipFile(target,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as dst:
   for name in sorted(names):
    path=R/pathlib.PurePosixPath(name).relative_to('PS4-FrameGen')
    # Include the final source, preserve original archive separately.
    dst.write(path,name)
 with zipfile.ZipFile(target) as z:
  assert z.testzip() is None
  for name,expected in required.items():assert sha(z.read('PS4-FrameGen/'+name))==expected,name
  for name in z.namelist():
   assert sha(z.read(name))==sha((R/pathlib.PurePosixPath(name).relative_to('PS4-FrameGen')).read_bytes()),name
 manifest=json.loads((D/'manifest.json').read_text())
 manifest['files'][target.name]=sha(target.read_bytes());manifest['primary_project_zip']=target.name
 manifest['superseded_project_zip']=old.name
 manifest['source_and_artifact_files_in_final_zip']=len(names)
 (D/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Final ZIP:',len(names),'files,',target.stat().st_size,'bytes; all manifest entries and source bytes verified')
if __name__=='__main__':main()
