// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

namespace hydra
{
    template <typename TRegister>
    struct SimdRegisterBase
    {
        TRegister v;

        SimdRegisterBase() = default;

        HYDRA_FORCE_INLINE
        SimdRegisterBase(TRegister In) : v(In)
        {
        }

        HYDRA_FORCE_INLINE
        operator TRegister() const
        {
            return v;
        }
    protected:
        using Super = SimdRegisterBase;
    };
} // namespace hydra
