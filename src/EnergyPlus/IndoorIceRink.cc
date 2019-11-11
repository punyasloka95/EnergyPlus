// EnergyPlus, Copyright (c) 1996-2019, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers
#include <cassert>
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>
#include <ObjexxFCL/gio.hh>
#include <ObjexxFCL/string.functions.hh>

// EnergyPlus Headers
#include <BranchNodeConnections.hh>
#include <DataBranchAirLoopPlant.hh>
#include <DataConversions.hh>
#include <DataEnvironment.hh>
#include <DataGlobals.hh>
#include <DataHVACGlobals.hh>
#include <DataHeatBalFanSys.hh>
#include <DataHeatBalSurface.hh>
#include <DataHeatBalance.hh>
#include <DataPlant.hh>
#include <DataPrecisionGlobals.hh>
#include <DataSizing.hh>
#include <DataSurfaces.hh>
#include <FluidProperties.hh>
#include <General.hh>
#include <GlobalNames.hh>
#include <HeatBalanceSurfaceManager.hh>
#include <IndoorIceRink.hh>
#include <InputProcessing/InputProcessor.hh>
#include <NodeInputManager.hh>
#include <OutputProcessor.hh>
#include <PlantUtilities.hh>
#include <Psychrometrics.hh>
#include <ScheduleManager.hh>
#include <UtilityRoutines.hh>

namespace EnergyPlus {
namespace IceRink {
    // USE STATEMENTS:
    // Use statements for data only modules
    // Using/Aliasing

    // Data
    // MODULE PARAMETER DEFINITIONS:
    // System types:
    static std::string const BlankString;
    std::string const cDRink("IndoorIceRink:DirectRefrigerationSystem");
    std::string const cIRink("IndoorIceRink:IndirectRefrigerationSystem");
    int const DirectSystem(1);   // Direct refrigeration type radiant system
    int const IndirectSystem(2); // Indirect refrigeration type radiant system

    // Fluid types in indirect refrigeration system
    int const CaCl2(1); // Calcium chloride solution
    int const EG(2);    // Ethylene Glycol solution

    // Control types:
    int const SurfaceTempControl(1);     // Controls system using ice surface temperature
    int const BrineOutletTempControl(2); // Controls system using brine outlet temperature
                                         // Limit temperatures to indicate that a system cannot cool
    Real64 HighTempCooling(200.0);       // Used to indicate that a user does not have a cooling control temperature

    // Operating Mode:
    int NotOperating(0); // Parameter for use with OperatingMode variable, set for not operating
    int CoolingMode(2);  // Parameter for use with OperatingMode variable, set for cooling

    bool GetInput(true);

    // Object Data
    Array1D<IceRinkData> Rink;
    Array1D<ResurfacerData> Resurfacer;
    std::unordered_map<std::string, std::string> UniqueRinkNames;

    // Functions:
    void clear_state()
    {
        GetInput = true;
        Rink.deallocate();
        Resurfacer.deallocate();
        UniqueRinkNames.clear();
    }

    PlantComponent *IceRinkData::factory(std::string const &objectName)
    {
        // Process the input data for ice rinks if it hasn't been done yet
        if (GetInput) {
            GetIndoorIceRink();
            GetInput = false;
        }
        // Now look for this particular rink in the list
        for (auto &rink : Rink) {
            if (rink.Name == objectName) {
                return &rink;
            }
        }
        // If it is not found, fatal error
        ShowFatalError("IceRinkFactory: Error getting inputs for rink named: " + objectName);
        // Shut up the compiler
        return nullptr;
    }

    void IceRinkData::simulate(const PlantLocation &EP_UNUSED(calledFromLocation),
                           bool const EP_UNUSED(FirstHVACIteration),
                           Real64 &CurLoad,
                           bool const RunFlag)
    {
        this->initialize();
        if (this->RinkType_Num == DataPlant::TypeOf_IceRink) {
            this->calculateIceRink(CurLoad);
        }
        this->update();
        this->report(RunFlag);
    }

    void GetIndoorIceRink()
    {
    }

    void IceRinkData::initialize()
    {
    }

    void IceRinkData::setupOutputVariables()
    {
    }

    Real64 IceRinkData::IceRinkFreezing(Real64 &FreezingLoad)
    {
        Real64 QFusion(333550.00);
        Real64 CpIce(2108.00);
        static std::string const RoutineName("IceRinkFreezing");

        Real64 RhoWater = FluidProperties::GetDensityGlycol("WATER", this->FloodWaterTemp, this->WaterIndex, RoutineName);
        Real64 CpWater = FluidProperties::GetSpecificHeatGlycol("WATER", this->FloodWaterTemp, this->WaterIndex, RoutineName);
        Real64 Volume = this->LengthRink * this->WidthRink * this->IceThickness;

        if (this->IceSetptSchedPtr > 0) {
            Real64 QFreezing = 0.001 * RhoWater * Volume *
                               ((CpWater * FloodWaterTemp) + (QFusion) - (CpIce * ScheduleManager::GetCurrentScheduleValue(this->IceSetptSchedPtr)));
            FreezingLoad = QFreezing;
        }

        return (FreezingLoad);
    }

    Real64 ResurfacerData::RinkResurfacer(Real64 &ResurfacingLoad)
    {
        static std::string const RoutineName("IceRinkResurfacer");
        Real64 QFusion(333550.00);
        Real64 CpIce(2108.00);
        Real64 MolarMassWater(18.015);
        Real64 RhoWater = FluidProperties::GetDensityGlycol("WATER", this->ResurfacingWaterTemp, this->WaterIndex, RoutineName);
        Real64 CpWater = FluidProperties::GetSpecificHeatGlycol("WATER", this->ResurfacingWaterTemp, this->WaterIndex, RoutineName);
        QResurfacing = this->NoOfResurfEvents * 0.001 * RhoWater * this->TankCapacity *
                       ((CpWater * this->ResurfacingWaterTemp) + (QFusion) - (CpIce * this->IceTemperature));
        EHeatingWater = 0.001 * this->TankCapacity * RhoWater * CpWater * (this->ResurfacingWaterTemp - this->InitWaterTemp);
        ResurfacingLoad = QResurfacing;
        return (ResurfacingLoad);
    }

    Real64 IceRinkData::calcEffectiveness(Real64 const Temperature,    // Temperature of refrigerant entering the floor radiant system, in C))
                                      Real64 const RefrigMassFlow) // Mass flow rate of refrigerant in the floor radiant system, in kg/s
    {
        // Using/Aliasing
        using DataGlobals::NumOfTimeStepInHour;
        using DataGlobals::Pi;
        using DataPlant::PlantLoop;

        // Return value
        Real64 CalcEffectiveness; // Function return variable

        // FUNCTION PARAMETER DEFINITIONS:
        Real64 const MaxLaminarRe(2300.0); // Maximum Reynolds number for laminar flow
        static std::string const RoutineName("IceRink:calcEffectiveness");
        Real64 const MaxExpPower(50.0);

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:

        Real64 NuseltNum; // Nuselt number (dimensionless)

        Real64 SpecificHeat =
            FluidProperties::GetSpecificHeatGlycol(PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        Real64 Conductivity =
            FluidProperties::GetConductivityGlycol(PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        Real64 Viscosity =
            FluidProperties::GetViscosityGlycol(PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        Real64 Density =
            FluidProperties::GetDensityGlycol(PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);

        // Calculate the Reynold's number from RE=(4*Mdot)/(Pi*Mu*Diameter)
        Real64 ReynoldsNum = 4.0 * RefrigMassFlow / (Pi * Viscosity * TubeDiameter * NumCircuits);

        Real64 PrantlNum = Viscosity * SpecificHeat / Conductivity;

        // Calculate the Nusselt number based on what flow regime one is in. h = (k)(Nu)/D
        if (ReynoldsNum >= MaxLaminarRe) { // Turbulent flow --> use Dittus-Boelter equation
            NuseltNum = 0.023 * std::pow(ReynoldsNum, 0.8) * std::pow(PrantlNum, 0.3);
        } else { // Laminar flow --> use constant surface temperature relation
            NuseltNum = 3.66;
        }

        Real64 NTU = Pi * Conductivity * NuseltNum * this->TubeLength / (RefrigMassFlow * SpecificHeat);

        if (NTU > MaxExpPower) {
            CalcEffectiveness = 1.0;
        } else {
            CalcEffectiveness = 1.0 - std::exp(-NTU);
        }

        return CalcEffectiveness;
    }

    void IceRinkData::calculateIceRink(Real64 &LoadMet)
    {
        // Using/Aliasing
        using DataHeatBalFanSys::CTFTsrcConstPart;
        using DataHeatBalFanSys::QRadSysSource;
        using DataHeatBalFanSys::RadSysTiHBConstCoef;
        using DataHeatBalFanSys::RadSysTiHBQsrcCoef;
        using DataHeatBalFanSys::RadSysTiHBToutCoef;
        using DataHeatBalFanSys::RadSysToHBConstCoef;
        using DataHeatBalFanSys::RadSysToHBQsrcCoef;
        using DataHeatBalFanSys::RadSysToHBTinCoef;
        using DataLoopNode::Node;
        using DataPlant::PlantLoop;
        using DataSurfaces::HeatTransferModel_CTF;
        using FluidProperties::GetSpecificHeatGlycol;
        using PlantUtilities::SetComponentFlowRate;
        using ScheduleManager::GetCurrentScheduleValue;

        static std::string const RoutineName("IceRink:calculateDirectIceRink");
        Real64 FreezingLoad;
        Real64 ResurfacingLoad;
        int SystemType = DirectSystem;
        int ZoneNum = this->ZonePtr;
        int ControlStrategy = this->ControlStrategy;
        int RefrigNodeIn = this->RefrigInNode;
        int SurfNum = this->SurfacePtr;

        if (RefrigNodeIn == 0) {
            ShowSevereError("Illegal inlet node for the refrigerant in the direct system");
            ShowFatalError("Preceding condition causes termination");
        }

        if (ControlStrategy == BrineOutletTempControl) {

            // Setting the set point temperature of refrigerant outlet
            if (this->RefrigSetptSchedPtr > 0) {
                this->RefrigSetptTemp = GetCurrentScheduleValue(this->RefrigSetptSchedPtr);
            } else {
                ShowSevereError("Illegal pointer to brine outlet control strategy");
                ShowFatalError("Preceding condition causes termination");
            }
        } else if (ControlStrategy == SurfaceTempControl) {

            // Setting the set point temperature of ice surface
            if (this->IceSetptSchedPtr > 0) {
                this->IceSetptTemp = GetCurrentScheduleValue(this->IceSetptSchedPtr);
            } else {
                ShowSevereError("Illegal pointer to surface temperature control strategy");
                ShowFatalError("Preceding condition causes termination");
            }
        } else {
            ShowSevereError("Illegal input for control strategy");
            ShowFatalError("Preceding condition causes termination");
        }

        Real64 RefrigMassFlow = Node(RefrigNodeIn).MassFlowRate;
        Real64 RefrigTempIn = Node(RefrigNodeIn).Temp;

        if (RefrigMassFlow <= 0) {
            // No flow or below minimum allowed so there is no heat source/sink
            // This is possible with a mismatch between system and plant operation
            // or a slight mismatch between zone and system controls.  This is not
            // necessarily a "problem" so this exception is necessary in the code.

            QRadSysSource(SurfNum) = 0.0;
            Real64 ReqMassFlow = 0.0;
            SetComponentFlowRate(ReqMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
        } else {
            // Refrigerant mass flow rate is significant

            // Determine the heat exchanger "effectiveness"
            Real64 Effectiveness = calcEffectiveness(RefrigTempIn, RefrigMassFlow);
            int ConstrNum = DataSurfaces::Surface(SurfNum).Construction;
            Real64 CpRefrig =
                GetSpecificHeatGlycol(PlantLoop(this->LoopNum).FluidName, RefrigTempIn, PlantLoop(this->LoopNum).FluidIndex, RoutineName);

            Real64 Ca = RadSysTiHBConstCoef(SurfNum);
            Real64 Cb = RadSysTiHBToutCoef(SurfNum);
            Real64 Cc = RadSysTiHBQsrcCoef(SurfNum);

            Real64 Cd = RadSysToHBConstCoef(SurfNum);
            Real64 Ce = RadSysToHBTinCoef(SurfNum);
            Real64 Cf = RadSysToHBQsrcCoef(SurfNum);

            Real64 Cg = CTFTsrcConstPart(SurfNum);
            Real64 Ch = DataHeatBalance::Construct(ConstrNum).CTFTSourceQ(0);
            Real64 Ci = DataHeatBalance::Construct(ConstrNum).CTFTSourceIn(0);
            Real64 Cj = DataHeatBalance::Construct(ConstrNum).CTFTSourceOut(0);

            Real64 Ck = Cg + ((Ci * (Ca + Cb * Cd) + Cj * (Cd + Ce * Ca)) / (1.0 - Ce * Cb));
            Real64 Cl = Ch + ((Ci * (Cc + Cb * Cf) + Cj * (Cf + Ce * Cc)) / (1.0 - Ce * Cb));

            QRadSysSource(SurfNum) =
                (RefrigTempIn - Ck) / ((Cl / DataSurfaces::Surface(SurfNum).Area) + (1 / (Effectiveness * RefrigMassFlow * CpRefrig)));

            Real64 TSource = Ck + (Cl * QRadSysSource(SurfNum));
            Real64 IceTemp = (Ca + (Cb * Cd) + (QRadSysSource(SurfNum) * (Cc + (Cb * Cf)))) / (1 - (Cb * Ce));
            Real64 QRadSysSourceMax = RefrigMassFlow * CpRefrig * (RefrigTempIn - TSource);

            if (ControlStrategy == BrineOutletTempControl) {
                // Finding the required mass flow rate so that the outlet temperature
                // becomes equal to the user defined refrigerant outlet temperature
                // for the given refrigerant inlet temperature.

                Real64 ReqMassFlow = (((Ck - RefrigTempIn) / (this->RefrigSetptTemp - RefrigTempIn)) - (1 / Effectiveness)) *
                                     (DataSurfaces::Surface(SurfNum).Area / (CpRefrig * Cl));

                Real64 TRefigOutCheck = RefrigTempIn - ((QRadSysSource(SurfNum)) / RefrigMassFlow * CpRefrig);

                if (TRefigOutCheck <= (this->RefrigSetptTemp)) {
                    RefrigMassFlow = this->MinRefrigMassFlow;
                    SetComponentFlowRate(
                        RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                } else {
                    if (ReqMassFlow >= this->MaxRefrigMassFlow) {
                        RefrigMassFlow = this->MaxRefrigMassFlow;
                        SetComponentFlowRate(
                            RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                    } else {
                        RefrigMassFlow = ReqMassFlow;
                        SetComponentFlowRate(
                            RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                    }
                }
            } else if (ControlStrategy == SurfaceTempControl) {
                // Finding heat thant needs to be extraced so that the
                // ice surface reaches the set point temperature.
                Real64 QSetPoint =
                    ((((1 - (Cb * Ce)) * this->IceSetptTemp) - Ca - (Cb * Cd)) / (Cc + (Cb * Cf))) * DataSurfaces::Surface(SurfNum).Area;

                Real64 ReqMassFlow = QSetPoint / ((Effectiveness * CpRefrig) * (RefrigTempIn - TSource));

                if (IceTemp <= this->IceSetptTemp) {
                    QRadSysSource(SurfNum) = 0.0;
                    RefrigMassFlow = 0.0;
                    SetComponentFlowRate(
                        RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                } else {
                    if (QRadSysSourceMax <= QSetPoint) {
                        RefrigMassFlow = this->MaxRefrigMassFlow;
                        SetComponentFlowRate(
                            RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                    } else {
                        RefrigMassFlow = ReqMassFlow;
                        SetComponentFlowRate(
                            RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                    }
                }
            }
            this->RefrigMassFlow = RefrigMassFlow;
        }

        // "Temperature Comparision" cut-off
        // Check if QRadSysSource is positive i.e. it is actually heating the rink.
        // If so then the refrigeration system is doing opposite of its intension
        // and it needs to be shut down.

        if (QRadSysSource(SurfNum) >= 0.0) {
            RefrigMassFlow = 0.0;
            SetComponentFlowRate(
                RefrigMassFlow, this->RefrigInNode, this->RefrigOutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
            this->RefrigMassFlow = RefrigMassFlow;
        }

        HeatBalanceSurfaceManager::CalcHeatBalanceOutsideSurf(ZoneNum);
        HeatBalanceSurfaceManager::CalcHeatBalanceInsideSurf(ZoneNum);

        IceRinkFreezing(FreezingLoad);
        for (auto &R : Resurfacer) {
            R.RinkResurfacer(ResurfacingLoad);
        }
        LoadMet = FreezingLoad + ResurfacingLoad;
    }

    void IceRinkData::update()
    {
    }

    void IceRinkData::report(bool RunFlag)
    {
    }
} // namespace IceRink
} // namespace EnergyPlus
