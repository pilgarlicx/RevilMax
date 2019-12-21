/*    Revil Tool for 3ds Max
      Copyright(C) 2019 Lukas Cone

      This program is free software : you can redistribute it and / or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation, either version 3 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program.If not, see <https://www.gnu.org/licenses/>.

      Revil Tool uses RevilLib 2017-2019 Lukas Cone
*/
#include "datas/reflector.hpp"

#include "LMT.h"
#include "RevilMax.h"
#include "datas/VectorsSimd.hpp"
#include "datas/esstring.h"
#include "datas/masterprinter.hpp"
#include <map>

#define MTFImport_CLASS_ID Class_ID(0x46f85524, 0xd4337f2)
static const TCHAR _className[] = _T("MTFImport");

class MTFImport : public SceneImport, RevilMax {
public:
  // Constructor/Destructor
  MTFImport();
  virtual ~MTFImport() {}

  virtual int ExtCount();          // Number of extensions supported
  virtual const TCHAR *Ext(int n); // Extension #n (i.e. "3DS")
  virtual const TCHAR *
  LongDesc(); // Long ASCII description (i.e. "Autodesk 3D Studio File")
  virtual const TCHAR *
  ShortDesc(); // Short ASCII description (i.e. "3D Studio")
  virtual const TCHAR *AuthorName();       // ASCII Author name
  virtual const TCHAR *CopyrightMessage(); // ASCII Copyright message
  virtual const TCHAR *OtherMessage1();    // Other message #1
  virtual const TCHAR *OtherMessage2();    // Other message #2
  virtual unsigned int Version();    // Version number * 100 (i.e. v3.01 = 301)
  virtual void ShowAbout(HWND hWnd); // Show DLL's "About..." box
  virtual int DoImport(const TCHAR *name, ImpInterface *i, Interface *gi,
                       BOOL suppressPrompts = FALSE); // Import file

  void LoadMotion(const LMTAnimation &mot, TimeValue startTime = 0);
};

class : public ClassDesc2 {
public:
  virtual int IsPublic() { return TRUE; }
  virtual void *Create(BOOL) { return new MTFImport(); }
  virtual const TCHAR *ClassName() { return _className; }
  virtual SClass_ID SuperClassID() { return SCENE_IMPORT_CLASS_ID; }
  virtual Class_ID ClassID() { return MTFImport_CLASS_ID; }
  virtual const TCHAR *Category() { return NULL; }
  virtual const TCHAR *InternalName() { return _className; }
  virtual HINSTANCE HInstance() { return hInstance; }
} MTFImportDesc;

ClassDesc2 *GetMTFImportDesc() { return &MTFImportDesc; }

MTFImport::MTFImport() {}

int MTFImport::ExtCount() { return 5; }

const TCHAR *MTFImport::Ext(int n) {
  switch (n) {
  case 0:
    return _T("lmt");
  case 1:
    return _T("tml");
  case 2:
    return _T("mlx");
  case 3:
    return _T("mtx");
  case 4:
    return _T("mti");
  default:
    return nullptr;
  }

  return nullptr;
}

const TCHAR *MTFImport::LongDesc() { return _T("MT Framework Import"); }

const TCHAR *MTFImport::ShortDesc() { return _T("MT Framework Import"); }

const TCHAR *MTFImport::AuthorName() { return _T("Lukas Cone"); }

const TCHAR *MTFImport::CopyrightMessage() {
  return _T("Copyright (C) 2019 Lukas Cone");
}

const TCHAR *MTFImport::OtherMessage1() { return _T(""); }

const TCHAR *MTFImport::OtherMessage2() { return _T(""); }

unsigned int MTFImport::Version() { return REVILMAX_VERSIONINT; }

void MTFImport::ShowAbout(HWND hWnd) { ShowAboutDLG(hWnd); }

struct LMTNode {
  DECLARE_REFLECTOR;
  union {
    struct {
      Vector r1, r2, r3, r4;
    };
    Matrix3 mtx;
  };
  INode *nde;
  int LMTBone;

  LMTNode(INode *input) : nde(input) {
    ReflectorWrap<LMTNode> refl(this);
    const int numRefl = refl.GetNumReflectedValues();
    bool corrupted = false;

    for (int r = 0; r < numRefl; r++) {
      Reflector::KVPair reflPair = refl.GetReflectedPair(r);
      TSTRING usName = esStringConvert<TCHAR>(reflPair.name);

      if (!nde->UserPropExists(usName.c_str())) {
        corrupted = true;
        break;
      }

      MSTR value;
      nde->GetUserPropString(usName.c_str(), value);

      refl.SetReflectedValue(reflPair.name,
                             esStringConvert<char>(value.data()).c_str());
    }

    if (!corrupted)
      return;

    mtx = nde->GetNodeTM(0);

    if (nde->GetParentNode()->IsRootNode()) {
      Matrix3 invCormat = corMat;
      invCormat.Invert();
      mtx *= invCormat;
    } else {
      Matrix3 invParent = nde->GetParentTM(0);
      invParent.Invert();
      mtx *= invParent;
    }

    for (int r = 0; r < numRefl; r++) {
      Reflector::KVPair reflPair = refl.GetReflectedPair(r);
      TSTRING usName = esStringConvert<TCHAR>(reflPair.name);
      TSTRING usVal = esStringConvert<TCHAR>(reflPair.value.c_str());

      nde->SetUserPropString(usName.c_str(), usVal.c_str());
    }
  }
};

REFLECTOR_CREATE(LMTNode, 1, VARNAMES, LMTBone, r1, r2, r3, r4);

static class : public ITreeEnumProc {
  const MSTR boneNameHint = _T("LMTBone");

public:
  std::vector<LMTNode> bones;

  void RescanBones() {
    bones.clear();
    GetCOREInterface7()->GetScene()->EnumTree(this);

    for (auto &b : bones) {
      if (b.LMTBone == -1)
        return;
    }

    for (auto &b : bones) {
      if (b.LMTBone == 255) {
        b.nde->SetUserPropInt(boneNameHint, -1);
        b.LMTBone = -1;
      }
    }
  }

  INode *LookupNode(int ID) {
    for (auto &b : bones) {
      if (b.LMTBone == ID)
        return b.nde;
    }

    return nullptr;
  }

  int callback(INode *node) {
    if (node->UserPropExists(boneNameHint)) {
      bones.push_back(node);
    }

    return TREE_CONTINUE;
  }
} iBoneScanner;

void MTFImport::LoadMotion(const LMTAnimation &mot, TimeValue startTime) {
  const int numTracks = mot.NumTracks();
  SetFrameRate(60);
  const float aDuration = mot.NumFrames() / 60.f;

  TimeValue numTicks = SecToTicks(aDuration);
  TimeValue ticksPerFrame = GetTicksPerFrame();
  TimeValue overlappingTicks = numTicks % ticksPerFrame;

  if (overlappingTicks > (ticksPerFrame / 2))
    numTicks += ticksPerFrame - overlappingTicks;
  else
    numTicks -= overlappingTicks;

  Interval aniRange(0, numTicks);
  GetCOREInterface()->SetAnimRange(aniRange);

  /*std::vector<float> frameTimes;

  for (TimeValue v = 0; v <= aniRange.End(); v += GetTicksPerFrame())
    frameTimes.push_back(TicksToSec(v));*/

  for (int t = 0; t < numTracks; t++) {
    const LMTTrack *tck = mot.Track(t);
    const int boneID = tck->AnimatedBoneID();
    const int numFrames = tck->NumFrames();

    INode *fNode = iBoneScanner.LookupNode(boneID);

    if (!fNode) {
      printwarning("[MTF] Couldn't find LMTBone: ", << boneID);
      continue;
    }

    Control *cnt = fNode->GetTMController();
    const LMTTrack::TrackType tckType = tck->GetTrackType();

    switch (tckType) {
    case LMTTrack::TrackType_AbsolutePosition:
    case LMTTrack::TrackType_LocalPosition: {
      if (cnt->GetPositionController()->ClassID() !=
          Class_ID(LININTERP_POSITION_CLASS_ID, 0))
        cnt->SetPositionController((Control *)CreateInstance(
            CTRL_POSITION_CLASS_ID, Class_ID(LININTERP_POSITION_CLASS_ID, 0)));

      Control *posCnt = cnt->GetPositionController();
      IKeyControl *kCon = GetKeyControlInterface(posCnt);

      kCon->SetNumKeys(numFrames);

      for (int f = 0; f < numFrames; f++) {
        TimeValue atTime = startTime + tck->GetFrame(f) * GetTicksPerFrame();

        Vector4A16 cVal;
        tck->Evaluate(cVal, f);

        ILinPoint3Key cKey;
        cKey.time = atTime;
        cKey.val = reinterpret_cast<Point3 &>(cVal) * IDC_EDIT_SCALE_value;

        if (fNode->GetParentNode()->IsRootNode() ||
            tckType == LMTTrack::TrackType_AbsolutePosition)
          cKey.val = corMat.PointTransform(cKey.val);

        kCon->SetKey(f, &cKey);
      }

      break;
    }
    case LMTTrack::TrackType_AbsoluteRotation:
    case LMTTrack::TrackType_LocalRotation: {
      if (cnt->GetRotationController()->ClassID() !=
          Class_ID(LININTERP_ROTATION_CLASS_ID, 0))
        cnt->SetRotationController((Control *)CreateInstance(
            CTRL_ROTATION_CLASS_ID, Class_ID(LININTERP_ROTATION_CLASS_ID, 0)));

      Control *rotCnt = (Control *)CreateInstance(
          CTRL_ROTATION_CLASS_ID, Class_ID(HYBRIDINTERP_ROTATION_CLASS_ID, 0));
      IKeyControl *kCon = GetKeyControlInterface(rotCnt);

      kCon->SetNumKeys(numFrames);

      for (int f = 0; f < numFrames; f++) {
        TimeValue atTime = startTime + tck->GetFrame(f) * GetTicksPerFrame();

        Vector4A16 cVal;
        tck->Evaluate(cVal, f);

        ILinRotKey cKey;
        cKey.time = atTime;
        cKey.val = reinterpret_cast<Quat &>(cVal).Conjugate();

        if (fNode->GetParentNode()->IsRootNode() ||
            tckType == LMTTrack::TrackType_AbsoluteRotation) {
          Matrix3 cMat;
          cMat.SetRotate(cKey.val);
          cKey.val = cMat * corMat;
        }

        kCon->SetKey(f, &cKey);
      }

      cnt->GetRotationController()->Copy(rotCnt); // Gimbal fix

      break;
    }
    case LMTTrack::TrackType_LocalScale: {
      if (fNode->NumberOfChildren()) {
        printwarning("Scale on bone with children:", << fNode->GetName());
        break;
      }

      if (cnt->GetScaleController()->ClassID() !=
          Class_ID(LININTERP_SCALE_CLASS_ID, 0))
        cnt->SetScaleController((Control *)CreateInstance(
            CTRL_SCALE_CLASS_ID, Class_ID(LININTERP_SCALE_CLASS_ID, 0)));

      Control *sclCnt = cnt->GetScaleController();
      IKeyControl *kCon = GetKeyControlInterface(sclCnt);

      kCon->SetNumKeys(numFrames);

      for (int f = 0; f < numFrames; f++) {
        TimeValue atTime = startTime + tck->GetFrame(f) * GetTicksPerFrame();

        Vector4A16 cVal;
        tck->Evaluate(cVal, f);

        ILinPoint3Key cKey;
        cKey.time = atTime;
        cKey.val = reinterpret_cast<Point3 &>(cVal);

        if (fNode->GetParentNode()->IsRootNode())
          cKey.val = corMat.PointTransform(cKey.val);

        kCon->SetKey(f, &cKey);
      }

      break;
    }

    default:
      break;
    }
  }

  /*SuspendAnimate();
  AnimateOn();

  for (auto b : iBoneScanner.bones) {
    INode *ffNode = b.nde;

    if (!ffNode->GetParentNode()->IsRootNode()) {
      Control *cnt = ffNode->GetTMController();

      if (cnt->GetPositionController()->ClassID() !=
          Class_ID(LININTERP_POSITION_CLASS_ID, 0))
        cnt->SetPositionController((Control *)CreateInstance(
            CTRL_POSITION_CLASS_ID, Class_ID(LININTERP_POSITION_CLASS_ID, 0)));

      if (cnt->GetRotationController()->ClassID() !=
          Class_ID(LININTERP_ROTATION_CLASS_ID, 0))
        cnt->SetRotationController((Control *)CreateInstance(
            CTRL_ROTATION_CLASS_ID, Class_ID(LININTERP_ROTATION_CLASS_ID, 0)));

      if (cnt->GetScaleController()->ClassID() !=
          Class_ID(LININTERP_SCALE_CLASS_ID, 0))
        cnt->SetScaleController((Control *)CreateInstance(
            CTRL_SCALE_CLASS_ID, Class_ID(LININTERP_SCALE_CLASS_ID, 0)));

      for (TimeValue v = 0; v <= aniRange.End(); v += GetTicksPerFrame()) {
        //Matrix3 pAbsMat = fNode->GetParentTM(v);
        //Point3 nScale = {pAbsMat.GetRow(0).Length(),
  pAbsMat.GetRow(1).Length(),
        //                 pAbsMat.GetRow(2).Length()};

        //pAbsMat.Invert();

        //for (int s = 0; s < 3; s++)
        //  if (!nScale[s])
        //    nScale[s] = FLT_EPSILON;

        Matrix3 cMat = ffNode->GetNodeTM(v);
       // *pAbsMat;
        //Point3 fracPos = cMat.GetTrans() / nScale;
        //nScale = 1.f - nScale;
        //cMat.Translate(fracPos * nScale);

        SetXFormPacket packet(cMat);

        cnt->SetValue(v, &packet);

        // cnt->GetPositionController()->SetValue(v, &localPos);
      }
    }
  }

  AnimateOff();*/
}

int MTFImport::DoImport(const TCHAR *fileName, ImpInterface * /*importerInt*/,
                        Interface * /*ip*/, BOOL suppressPrompts) {
  char *oldLocale = setlocale(LC_NUMERIC, NULL);
  setlocale(LC_NUMERIC, "en-US");

  TSTRING _filename = fileName;
  LMT mainAsset;

  if (mainAsset.Load(esStringConvert<char>(fileName).c_str(), true))
    return FALSE;

  int curMotionID = 0;

  for (auto &m : mainAsset) {
    if (m)
      motionNames.push_back(ToTSTRING(curMotionID));
    else
      motionNames.push_back(_T("--[Empty]--"));

    curMotionID++;
  }

  if (!suppressPrompts)
    if (!SpawnDialog())
      return TRUE;

  const LMTAnimation *mot = mainAsset.Animation(IDC_CB_MOTION_index);

  if (!mot)
    return FALSE;

  iBoneScanner.RescanBones();
  LoadMotion(*mot);

  setlocale(LC_NUMERIC, oldLocale);

  return TRUE;
}
