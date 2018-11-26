#pragma once
#include "Command.h"

namespace Shizuku{ namespace Flow{ namespace Command{
    class FLOW_API SetViscosity : public Command
    {
    public:
        SetViscosity(Flow& p_flow);
        void Start(const float p_viscosity);
    };
} } }
