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
  INode *ikTarget = nullptr;
  int LMTBone;

  INode *GetNode() { return ikTarget ? ikTarget : nde; }

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

    BOOL isNub = 0;

    if (nde->GetUserPropBool(_T("isnub"), isNub)) {
      TSTRING bneName = nde->GetName();

      if (isNub) {
        ikTarget = GetCOREInterface()->GetINodeByName(
            (bneName + _T("_IKTarget")).c_str());
      } else {
        ikTarget = GetCOREInterface()->GetINodeByName(
            (bneName + _T("_IKNub")).c_str());
      }
    }

    if (!corrupted)
      return;

    mtx = nde->GetNodeTM(0);

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
public:
  const MSTR boneNameHint = _T("LMTBone");

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

  void RestoreBasePose() {
    for (auto &n : bones) {
      SuspendAnimate();
      AnimateOn();
      n.nde->SetNodeTM(-GetTicksPerFrame(), n.mtx);
      AnimateOff();
    }
  }

  void ResetScene() {
    for (auto &n : bones) {
      Control *cnt = n.nde->GetTMController();
      //cnt->GetScaleController()->DeleteKeys(TRACK_DOALL);
      //cnt->GetRotationController()->DeleteKeys(TRACK_DOALL);
      //cnt->GetPositionController()->DeleteKeys(TRACK_DOALL);

     if (cnt->GetRotationController()->ClassID() !=
          Class_ID(LININTERP_ROTATION_CLASS_ID, 0))
        cnt->SetRotationController((Control *)CreateInstance(
            CTRL_ROTATION_CLASS_ID, Class_ID(LININTERP_ROTATION_CLASS_ID, 0)));

      if (cnt->GetScaleController()->ClassID() !=
          Class_ID(LININTERP_SCALE_CLASS_ID, 0))
        cnt->SetScaleController((Control *)CreateInstance(
            CTRL_SCALE_CLASS_ID, Class_ID(LININTERP_SCALE_CLASS_ID, 0)));

      if (cnt->GetPositionController()->ClassID() !=
          Class_ID(LININTERP_POSITION_CLASS_ID, 0))
        cnt->SetPositionController((Control *)CreateInstance(
            CTRL_POSITION_CLASS_ID, Class_ID(LININTERP_POSITION_CLASS_ID, 0)));

      SuspendAnimate();
      AnimateOn();

      n.nde->SetNodeTM(0, n.mtx);

      AnimateOff();
    }
  }

  LMTNode *LookupNode(int ID) {
    for (auto &b : bones) {
      if (b.LMTBone == ID)
        return &b;
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

struct MTFTrackPair {
  INode *nde;
  const LMTTrack *track;
  INode *scaleNode;
  MTFTrackPair *parent;
  std::vector<Vector4A16> frames;
  std::vector<MTFTrackPair *> children;

  MTFTrackPair(INode *inde, const LMTTrack *tck, MTFTrackPair *prent = nullptr)
      : nde(inde), track(tck), scaleNode(nullptr), parent(prent) {}
};

typedef std::vector<MTFTrackPair> MTFTrackPairCnt;
typedef std::vector<TimeValue> Times;
typedef std::vector<float> Secs;

static bool IsRoot(MTFTrackPairCnt &collection, INode *item) {
  if (item->IsRootNode())
    return true;

  auto fnd = std::find_if(collection.begin(), collection.end(),
                          [item](const MTFTrackPair &i0) {
                            return i0.nde == item->GetParentNode();
                          });

  if (fnd != collection.end())
    return false;

  return IsRoot(collection, item->GetParentNode());
}

static void BuildScaleHandles(MTFTrackPairCnt &pairs, MTFTrackPair &item,
                              const Times &times) {
  INode *fNode = item.nde;
  int numChildren = fNode->NumberOfChildren();

  for (int c = 0; c < numChildren; c++) {
    int LMTIndex;
    INode *childNode = fNode->GetChildNode(c);

    if (childNode->GetUserPropInt(iBoneScanner.boneNameHint, LMTIndex) &&
        LMTIndex == -2) {
      item.scaleNode = childNode;
      break;
    }
  }

  if (!item.scaleNode) {
    item.scaleNode = item.nde;

    Object *obj = static_cast<Object *>(
        CreateInstance(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0)));
    item.nde = GetCOREInterface()->CreateObjectNode(obj);
    item.nde->ShowBone(2);
    item.nde->SetWireColor(0x80ff);
    Matrix3 gt = item.scaleNode->GetNodeTM(0);
    item.nde->SetNodeTM(0, gt);

    TSTRING bName = item.scaleNode->GetName();
    bName.append(_T("_sp"));

    item.nde->SetName(ToBoneName(bName));

    LMTNode lNode(item.scaleNode);

    item.nde->SetUserPropInt(iBoneScanner.boneNameHint, lNode.LMTBone);

    item.scaleNode->SetUserPropInt(iBoneScanner.boneNameHint, -2);
    item.scaleNode->GetParentNode()->AttachChild(item.nde);
  }

  for (int c = 0; c < numChildren; c++)
    item.nde->AttachChild(item.scaleNode->GetChildNode(c));

  item.nde->AttachChild(item.scaleNode);

  item.frames.resize(times.size(), Vector4A16(1.f));

  item.scaleNode->GetTMController()->GetRotationController()->DeleteKeys(
      TRACK_DOALL);
  item.scaleNode->GetTMController()->GetPositionController()->DeleteKeys(
      TRACK_DOALL);

  Matrix3 pTM = item.nde->GetNodeTM(0);

  AnimateOn();
  item.scaleNode->SetNodeTM(0, pTM);
  AnimateOff();

  fNode = item.nde;
  numChildren = fNode->NumberOfChildren();

  for (int c = 0; c < numChildren; c++) {
    int LMTIndex;
    INode *childNode = fNode->GetChildNode(c);

    if (childNode->GetUserPropInt(iBoneScanner.boneNameHint, LMTIndex) &&
        LMTIndex == -2) {
      continue;
    } else {
      const LMTTrack *foundTrack = nullptr;

      for (auto &t : pairs)
        if (t.nde == childNode || t.scaleNode == childNode) {
          foundTrack = t.track;
          break;
        }

      MTFTrackPair *nChild = new MTFTrackPair(childNode, foundTrack, &item);
      item.children.push_back(nChild);
      BuildScaleHandles(pairs, **std::prev(item.children.end()), times);
    }
  }
}

static void PopulateScaleData(MTFTrackPair &item, const Times &times,
                              const Secs &secs) {
  if (!item.scaleNode)
    return;

  const size_t numKeys = times.size();
  Control *cnt = item.scaleNode->GetTMController()->GetScaleController();

  AnimateOn();

  if (item.track) {
    for (int t = 0; t < numKeys; t++) {
      Vector4A16 cVal;
      item.track->Interpolate(cVal, secs[t]);
      item.frames[t] *= cVal;

      if (item.parent)
        item.frames[t] *= item.parent->frames[t];

      Point3 kVal(item.frames[t].X, item.frames[t].Y, item.frames[t].Z);

      cnt->SetValue(times[t], &kVal);
    }
  }

  AnimateOff();

  for (auto &c : item.children) {
    PopulateScaleData(*c, times, secs);
  }
}

static void
FixupHierarchialTranslations(INode *nde, const Times &times,
                             const std::vector<Vector4A16> &scaleValues,
                             const std::vector<Matrix3> *parentTransforms) {
  const size_t numKeys = times.size();
  const int numChildren = nde->NumberOfChildren();
  std::vector<Point3> values;
  values.resize(numKeys);

  for (int c = 0; c < numChildren; c++) {
    INode *childNode = nde->GetChildNode(c);
    int LMTIndex;

    if (childNode->GetUserPropInt(iBoneScanner.boneNameHint, LMTIndex) &&
        LMTIndex == -2)
      continue;

    for (int t = 0; t < numKeys; t++) {
      Matrix3 rVal = childNode->GetNodeTM(times[t]);
      Matrix3 pVal = nde->GetNodeTM(times[t]);
      pVal.Invert();
      rVal *= pVal;
      values[t] = rVal.GetTrans() *
                  Point3(scaleValues[t].X, scaleValues[t].Y, scaleValues[t].Z);
    }

    /*for (int t = 0; t < numKeys; t++) {
      Matrix3 rVal = childNode->GetNodeTM(times[t]);
      Matrix3 pVal = parentTransforms->at(t);
      pVal.Invert();
      rVal *= pVal;
      rVal.SetTrans(rVal.GetTrans() * Point3(scaleValues[t].X, scaleValues[t].Y,
    scaleValues[t].Z)); pVal.Invert(); rVal *= pVal; pVal =
    nde->GetNodeTM(times[t]); pVal.Invert(); rVal *= pVal; values[t] =
    rVal.GetTrans();
    }*/

    Control *cnt = childNode->GetTMController()->GetPositionController();

    AnimateOn();

    for (int t = 0; t < numKeys; t++) {
      cnt->SetValue(times[t], &values[t]);
    }

    AnimateOff();
    //FixupHierarchialTranslations(childNode, times, scaleValues,
                                // parentTransforms);
  }
}

static void ScaleTranslations(MTFTrackPair &item, const Times &times) {
  if (!item.scaleNode)
    return;

  std::vector<Matrix3> absValues;
  absValues.reserve(times.size());

  for (auto t : times)
    absValues.push_back(item.nde->GetNodeTM(t));

  FixupHierarchialTranslations(item.nde, times, item.frames, &absValues);

  for (auto &c : item.children) {
    ScaleTranslations(*c, times);
  }
}

static void ApplyScaleTracks(MTFTrackPairCnt &tracks, const Times &times,
                             const Secs &secs) {
  std::vector<MTFTrackPair *> rootsOnly;

  for (auto &s : tracks)
    if (IsRoot(tracks, s.nde))
      rootsOnly.push_back(&s);

  for (auto &s : rootsOnly)
    BuildScaleHandles(tracks, *s, times);

  for (auto &s : rootsOnly)
    PopulateScaleData(*s, times, secs);

  for (auto &s : rootsOnly)
    ScaleTranslations(*s, times);
}

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

  Interval aniRange(0, numTicks - ticksPerFrame);
  GetCOREInterface()->SetAnimRange({-TIME_TICKSPERSEC, numTicks});
  std::vector<float> frameTimes;
  Times frameTimesTicks;

  for (TimeValue v = 0; v <= aniRange.End(); v += GetTicksPerFrame()) {
    frameTimes.push_back(TicksToSec(v));
    frameTimesTicks.push_back(v);
  }

  std::vector<MTFTrackPair> scaleTracks;

  for (int t = 0; t < numTracks; t++) {
    const LMTTrack *tck = mot.Track(t);
    const int boneID = tck->AnimatedBoneID();
    LMTNode *lNode = iBoneScanner.LookupNode(boneID);

    if (lNode && tck->GetTrackType() == LMTTrack::TrackType_LocalScale)
      scaleTracks.emplace_back(lNode->nde, tck);
  }

  std::vector<MTFTrackPair *> rootsOnly;

  for (auto &s : scaleTracks)
    if (IsRoot(scaleTracks, s.nde))
      rootsOnly.push_back(&s);

  for (auto &s : rootsOnly)
    BuildScaleHandles(scaleTracks, *s, frameTimesTicks);

  iBoneScanner.RescanBones();
  iBoneScanner.ResetScene();

  for (int t = 0; t < numTracks; t++) {
    const LMTTrack *tck = mot.Track(t);
    const int boneID = tck->AnimatedBoneID();
    LMTNode *lNode = iBoneScanner.LookupNode(boneID);

    if (!lNode) {
      printwarning("[MTF] Couldn't find LMTBone: ", << boneID);
      continue;
    }

    INode *fNode = lNode->GetNode();
    Control *cnt = fNode->GetTMController();
    const LMTTrack::TrackType tckType = tck->GetTrackType();

    switch (tckType) {
    case LMTTrack::TrackType_AbsolutePosition:
    case LMTTrack::TrackType_LocalPosition: {
      Control *posCnt = cnt->GetPositionController();

      AnimateOn();

      for (auto ft : frameTimes) {
        Vector4A16 cVal;
        tck->Interpolate(cVal, ft);
        cVal *= IDC_EDIT_SCALE_value;
        Point3 kVal = reinterpret_cast<Point3 &>(cVal);

        if (fNode->GetParentNode()->IsRootNode() ||
            tckType == LMTTrack::TrackType_AbsolutePosition)
          kVal = corMat.PointTransform(kVal);

        posCnt->SetValue(SecToTicks(ft), &kVal);
      }

      AnimateOff();
      break;
    }
    case LMTTrack::TrackType_AbsoluteRotation:
    case LMTTrack::TrackType_LocalRotation: {
      Control *rotCnt = (Control *)CreateInstance(
          CTRL_ROTATION_CLASS_ID, Class_ID(HYBRIDINTERP_ROTATION_CLASS_ID, 0));

      AnimateOn();

      for (auto ft : frameTimes) {
        Vector4A16 cVal;
        tck->Interpolate(cVal, ft);
        Quat kVal = reinterpret_cast<Quat &>(cVal).Conjugate();

        if (fNode->GetParentNode()->IsRootNode() ||
            tckType == LMTTrack::TrackType_AbsoluteRotation) {
          Matrix3 cMat;
          cMat.SetRotate(kVal);
          kVal = cMat * corMat;
        }

        rotCnt->SetValue(SecToTicks(ft), &kVal);
      }

      AnimateOff();
      cnt->GetRotationController()->Copy(rotCnt); // Gimbal fix

      break;
    }
    default:
      break;
    }
  }

  for (auto &s : rootsOnly)
    PopulateScaleData(*s, frameTimesTicks, frameTimes);

  for (auto &s : rootsOnly)
    ScaleTranslations(*s, frameTimesTicks);

  GetCOREInterface()->SetAnimRange(aniRange);
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
  iBoneScanner.RescanBones();
  iBoneScanner.RestoreBasePose();

  setlocale(LC_NUMERIC, oldLocale);

  return TRUE;
}
