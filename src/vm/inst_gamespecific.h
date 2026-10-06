#pragma once

#include "vm.h"

namespace Impacto {

namespace Vm {

VmInstruction(InstUnk0041);
VmInstruction(InstUnk0052);
VmInstruction(InstUnk0053);
VmInstruction(InstUnk0054);
VmInstruction(InstAddContents);
VmInstruction(InstUnk011F);
VmInstruction(InstRINNS);
VmInstruction(InstRINNSMain);
VmInstruction(InstUnk012D);
VmInstruction(InstAllClearChkCHLCC);
VmInstruction(InstChatMO8);
VmInstruction(InstGeotag);
VmInstruction(InstUnk100FMO6);
VmInstruction(InstUnk1010MO6);
VmInstruction(InstUnk1011MO6);
VmInstruction(InstUnk1037);
VmInstruction(InstUnk1037Noah);
VmInstruction(InstUnk1037MO7);
VmInstruction(InstMapSystem);
VmInstruction(InstPhoneSG);
VmInstruction(InstPhoneSGHD);
VmInstruction(InstScroll1038SGHD);
VmInstruction(InstMail);
VmInstruction(InstMtrg);

// STEINS;GATE Steam (SGHD). Operand layouts follow CoZ's sc3ntist SGHD
// decoder; behaviour is unknown unless noted, so most only consume operands.
VmInstruction(InstNopSGHD);
VmInstruction(InstByteArgSGHD);
VmInstruction(InstUnk004CSGHD);
VmInstruction(InstCallFarIfFlagSGHD);
VmInstruction(InstCallIfFlagSGHD);
VmInstruction(InstUselessJumpSGHD);
VmInstruction(InstUnk0106SGHD);
VmInstruction(InstUnk0107SGHD);
VmInstruction(InstCheckpointSGHD);
VmInstruction(InstUnk1038Darling);
VmInstruction(InstUnk1038MO7);
VmInstruction(InstTwipo);
VmInstruction(InstTwipo_Dash);
VmInstruction(InstDelusionTriggerCHLCC);
VmInstruction(InstYesNoTriggerCCLCC);
VmInstruction(InstLaunchApp);
VmInstruction(InstUnk103A);
VmInstruction(InstUnk2010CCLCC);
VmInstruction(InstUnk2011CCLCC);
VmInstruction(InstUnk2012CCLCC);

}  // namespace Vm

}  // namespace Impacto