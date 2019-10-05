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
    using namespace DataPrecisionGlobals;
    using DataGlobals::BeginTimeStepFlag;
    using DataGlobals::WarmupFlag;
    using DataHeatBalance::Construct;
    using DataHeatBalFanSys::QRadSysSource;
    using DataSurfaces::Surface;
    using DataSurfaces::TotSurfaces;
    using Psychrometrics::PsyTdpFnWPb;

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

    // Condensation control types:
    int const CondCtrlNone(0);      // Condensation control--none, so system never shuts down
    int const CondCtrlSimpleOff(1); // Condensation control--simple off, system shuts off when condensation predicted
    int const CondCtrlVariedOff(2); // Condensation control--variable off, system modulates to keep running if possible
    // Number of Circuits per Surface Calculation Method
    int const OneCircuit(1);          // there is 1 circuit per surface
    int const CalculateFromLength(2); // The number of circuits is TubeLength*SurfaceFlowFrac / CircuitLength
    std::string const OnePerSurf("OnePerSurface");
    std::string const CalcFromLength("CalculateFromCircuitLength");

    static std::string const fluidNameWater("WATER");
    static std::string const fluidNameBrine("BRINE");
    static std::string const fluidNameAmmonia("NH3");

    // MODULE VARIABLE DECLARATIONS:
    bool GetInputFlag = true;
    int TotalNumRefrigSystem(0); // Total number of refrigeration systems
    Array1D_bool CheckEquipName;
    int NumOfDirectRefrigSys(0);          // Number of direct refrigeration type ice rinks
    int NumOfIndirectRefrigSys(0);        // Number of indirect refrigeration type ice rinks
    bool FirstTimeInit(true);             // Set to true for first pass through init routine then set to false
    int OperatingMode(0);                 // Used to keep track of whether system is in heating or cooling mode
    Array1D<Real64> ZeroSourceSumHATsurf; // Equal to SumHATsurf for all the walls in a zone with no source
    Array1D<Real64> QRadSysSrcAvg;        // Average source over the time step for a particular radiant surface
    // Record keeping variables used to calculate QRadSysSrcAvg locally
    Array1D<Real64> LastQRadSysSrc;     // Need to keep the last value in case we are still iterating
    Array1D<Real64> LastSysTimeElapsed; // Need to keep the last value in case we are still iterating
    Array1D<Real64> LastTimeStepSys;    // Need to keep the last value in case we are still iterating

    // Object Data
    Array1D<DirectRefrigSysData> DRink;
    Array1D<IndirectRefrigSysData> IRink;
    Array1D<RefrigSysTypeData> RefrigSysTypes;
    Array1D<ResurfacerData> Resurfacer;

    // Functions:

    /* void SimIndoorIceRink(std::string &Name,             // name of the refrigeration system
                           std::string &ResurfacerName,   // name of the resurfacer
                           bool const FirstHVACIteration, // TRUE if 1st HVAC simulation of system timestep
                           Real64 &LoadMet,               // Load met by the refrigeration system, in Watts
                           int const RefrigType,          // Type of refrigerant used in the indirect type refrigeration system
                           int &CompIndex,
                           int &ResurfacerIndex)
     {
         // Using/Aliasing
         using General::TrimSigDigits;

         // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
         int SysNum;        // Refrigeration system number/index in local derived types
         int SystemType;    // Type of refrigeration system: Direct or Indirect
         int ResurfacerNum; // Resurfacer machine number/index in local derived types
         bool InitErrorFound(false);

         // FLOW:
         if (GetInputFlag) {
             GetIndoorIceRink();
             GetInputFlag = false;
         }

         // Find what type of system is it, is it a direct refrigeration system
         // or indirect refrigeration system ?
         if (CompIndex == 0) {
             SysNum = UtilityRoutines::FindItemInList(Name, RefrigSysTypes);
             if (SysNum == 0) {
                 ShowFatalError("SimIndoorIceRink: Unit not found =" + Name);
             }
             CompIndex = SysNum;
             SystemType = RefrigSysTypes(SysNum).SystemType;
             {
                 auto const SELECT_CASE_var(SystemType);
                 if (SELECT_CASE_var == DirectSystem) {
                     RefrigSysTypes(SysNum).CompIndex = UtilityRoutines::FindItemInList(Name, DRink);
                 } else if (SELECT_CASE_var == IndirectSystem) {
                     RefrigSysTypes(SysNum).CompIndex = UtilityRoutines::FindItemInList(Name, IRink);
                 }
             }

         } else {
             SysNum = CompIndex;
             SystemType = RefrigSysTypes(SysNum).SystemType;
             if (SysNum > TotalNumRefrigSystem || SysNum < 1) {
                 ShowFatalError("SimIndoorIceRink:  Invalid CompIndex passed=" + TrimSigDigits(SysNum) +
                                ", Number of Units=" + TrimSigDigits(TotalNumRefrigSystem) + ", Entered Unit name=" + Name);
             }
             if (CheckEquipName(SysNum)) {
                 if (Name != RefrigSysTypes(SysNum).Name) {
                     ShowFatalError("SimIndoorIceRink: Invalid CompIndex passed=" + TrimSigDigits(SysNum) + ", Unit name=" + Name +
                                    ", stored Unit Name for that index=" + RefrigSysTypes(SysNum).Name);
                 }
                 CheckEquipName(SysNum) = false;
             }
         }

         InitIndoorIceRink(FirstHVACIteration, InitErrorFound, SystemType, SysNum, ResurfacerIndex);
         if (InitErrorFound) {
             ShowFatalError("InitLowTempRadiantSystem: Preceding error is not allowed to proceed with the simulation.  Correct this input problem.");
         }

         {
             auto const SELECT_CASE_var(SystemType);
             if (SystemType == DirectSystem) {
                 CalcDirectIndoorIceRinkSys(RefrigSysTypes(SysNum).CompIndex, LoadMet);
             } else if (SystemType == IndirectSystem) {
                 CalcIndirectIndoorIceRinkSys(RefrigSysTypes(SysNum).CompIndex, LoadMet, RefrigType);
             } else {
                 ShowFatalError("SimIndoorIceRink: Illegal system type for system " + Name);
             }
         }

         UpdateIndoorIceRink(SysNum, SystemType);

         ReportIndoorIceRink(SysNum, SystemType);
     }*/

    void GetIndoorIceRink()
    {
        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine reads the input for indoor ice rinks from the user
        // input file.  This will contain all of the information
        // needed to simulate a indoor ice rink.

        // Using/Aliasing
        using BranchNodeConnections::TestCompSet;
        using DataGlobals::ScheduleAlwaysOn;
        using DataHeatBalance::Zone;
        using DataSizing::AutoSize;
        using DataSurfaces::HeatTransferModel_CTF;
        using DataSurfaces::SurfaceClass_Floor;
        using DataSurfaces::SurfaceClass_Window;
        using FluidProperties::FindGlycol;
        using NodeInputManager::GetOnlySingleNode;
        using ScheduleManager::GetScheduleIndex;
        using namespace DataLoopNode;

        // SUBROUTINE PARAMETER DEFINITIONS:
        Real64 const MinThrottlingRange(0.5); // Smallest throttling range allowed in degrees Celsius
        static std::string const RefrigOutletTemperature("RefrigOutletTemperature");
        static std::string const IceSurfaceTemperature("IceSurfaceTemperature");
        static std::string const RoutineName("GetIndoorIceRink");
        static std::string const Off("Off");
        static std::string const SimpleOff("SimpleOff");
        static std::string const VariableOff("VariableOff");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        static bool ErrorsFound(false);  // Set to true if something goes wrong
        std::string CurrentModuleObject; // for ease in getting objects
        Array1D_string Alphas;           // Alpha items for object
        Array1D_string cAlphaFields;     // Alpha field names
        Array1D_string cNumericFields;   // Numeric field names
        int GlycolIndex;                 // Index of 'Refrigerant' in glycol data structure
        int IOStatus;                    // Used in GetObjectItem
        int Item;                        // Item to be "gotten"
        int MaxAlphas;                   // Maximum number of alphas for these input keywords
        int MaxNumbers;                  // Maximum number of numbers for these input keywords
        Array1D<Real64> Numbers;         // Numeric items for object
        int NumAlphas;                   // Number of Alphas for each GetObjectItem call
        int NumArgs;                     // Unused variable that is part of a subroutine call
        int NumNumbers;                  // Number of Numbers for each GetObjectItem call
        int BaseNum;                     // Temporary number for creating RefrigSystemTypes structure
        Array1D_bool lAlphaBlanks;       // Logical array, alpha field input BLANK = .TRUE.
        Array1D_bool lNumericBlanks;     // Logical array, numeric field input BLANK = .TRUE.
        int SurfNum;                     // Surface number

        // FLOW:
        // Initializations and allocations
        MaxAlphas = 0;
        MaxNumbers = 0;

        inputProcessor->getObjectDefMaxArgs("IndoorIceRink:DirectRefrigSystem", NumArgs, NumAlphas, NumNumbers);
        MaxAlphas = max(MaxAlphas, NumAlphas);
        MaxNumbers = max(MaxNumbers, NumNumbers);

        inputProcessor->getObjectDefMaxArgs("IndoorIceRink:IndirectRefrigSystem", NumArgs, NumAlphas, NumNumbers);
        MaxAlphas = max(MaxAlphas, NumAlphas);
        MaxNumbers = max(MaxNumbers, NumNumbers);

        Alphas.allocate(MaxAlphas);
        Numbers.dimension(MaxNumbers, 0.0);
        cAlphaFields.allocate(MaxAlphas);
        cNumericFields.allocate(MaxNumbers);
        lAlphaBlanks.dimension(MaxAlphas, true);
        lNumericBlanks.dimension(MaxNumbers, true);

        NumOfDirectRefrigSys = inputProcessor->getNumObjectsFound("IndoorIceRink:DirectRefrigSystem");
        NumOfIndirectRefrigSys = inputProcessor->getNumObjectsFound("IndoorIceRink:IndirectRefrigSystem");

        TotalNumRefrigSystem = NumOfDirectRefrigSys + NumOfIndirectRefrigSys;
        RefrigSysTypes.allocate(TotalNumRefrigSystem);
        CheckEquipName.dimension(TotalNumRefrigSystem, true);

        DRink.allocate(NumOfDirectRefrigSys);
        if (NumOfDirectRefrigSys > 0) {
            GlycolIndex = FindGlycol(fluidNameAmmonia);
            for (auto &e : DRink)
                e.GlycolIndex = GlycolIndex;
            if (GlycolIndex == 0) {
                ShowSevereError("Direct Refrigeration systems: no refrigerant(ammonia) property data found in input");
                ErrorsFound = true;
            }
        } else {
            for (auto &e : DRink)
                e.GlycolIndex = 0;
        }

        IRink.allocate(NumOfIndirectRefrigSys);
        if (NumOfIndirectRefrigSys > 0) {
            GlycolIndex = FindGlycol(fluidNameBrine);
            for (auto &e : IRink)
                e.GlycolIndex = GlycolIndex;
            if (GlycolIndex == 0) {
                ShowSevereError("Indirect Refrigeration systems: no refrigerant(ammonia) property data found in input");
                ErrorsFound = true;
            }
        } else {
            for (auto &e : IRink)
                e.GlycolIndex = 0;
        }

        // Obtain all the user data related to direct refrigeration type indoor ice rink
        BaseNum = 0;
        CurrentModuleObject = "IndoorIceRink:DirectRefrigSystem";
        for (Item = 1; Item <= NumOfDirectRefrigSys; ++Item) {

            inputProcessor->getObjectItem(CurrentModuleObject,
                                          Item,
                                          Alphas,
                                          NumAlphas,
                                          Numbers,
                                          NumNumbers,
                                          IOStatus,
                                          lNumericBlanks,
                                          lAlphaBlanks,
                                          cAlphaFields,
                                          cNumericFields);
            ++BaseNum;
            RefrigSysTypes(BaseNum).Name = Alphas(1);
            RefrigSysTypes(BaseNum).SystemType = DirectSystem;
            // General user input data
            DRink(Item).Name = Alphas(1);
            DRink(Item).SchedName = Alphas(2);
            if (lAlphaBlanks(2)) {
                DRink(Item).SchedPtr = ScheduleAlwaysOn;
            } else {
                DRink(Item).SchedPtr = GetScheduleIndex(Alphas(2));
                if (DRink(Item).SchedPtr == 0) {
                    ShowSevereError(cAlphaFields(2) + " not found for " + Alphas(1));
                    ShowContinueError("Missing " + cAlphaFields(2) + " is " + Alphas(2));
                    ErrorsFound = true;
                }
            }

            DRink(Item).ZoneName = Alphas(3);
            DRink(Item).ZonePtr = UtilityRoutines::FindItemInList(Alphas(3), Zone);
            if (DRink(Item).ZonePtr == 0) {
                ShowSevereError(RoutineName + "Invalid " + cAlphaFields(3) + " = " + Alphas(3));
                ShowContinueError("Occurs in " + CurrentModuleObject + " = " + Alphas(1));
                ErrorsFound = true;
            }

            DRink(Item).SurfaceName = Alphas(4);
            DRink(Item).SurfacePtr = 0;
            for (SurfNum = 1; SurfNum <= TotSurfaces; ++SurfNum) {
                if (UtilityRoutines::SameString(Surface(SurfNum).Name, DRink(Item).SurfaceName)) {
                    DRink(Item).SurfacePtr = SurfNum;
                    break;
                }
            }
            if (DRink(Item).SurfacePtr <= 0) {
                ShowSevereError(RoutineName + "Invalid " + cAlphaFields(4) + " = " + Alphas(4));
                ShowContinueError("Occurs in " + CurrentModuleObject + " = " + Alphas(1));
                ErrorsFound = true;
            } else if (Surface(DRink(Item).SurfacePtr).HeatTransferAlgorithm != HeatTransferModel_CTF) {
                ShowSevereError(Surface(DRink(Item).SurfacePtr).Name +
                                " is an ice rink floor and is attempting to use a non-CTF solution algorithm.  This is "
                                "not allowed.  Use the CTF solution algorithm for this surface.");
                ErrorsFound = true;
            } else if (Surface(DRink(Item).SurfacePtr).Class == SurfaceClass_Window) {
                ShowSevereError(
                    Surface(DRink(Item).SurfacePtr).Name +
                    " is an ice rink floor and is defined as a window.  This is not allowed.  A pool must be a floor that is NOT a window.");
                ErrorsFound = true;
            } else if (Surface(DRink(Item).SurfacePtr).Class == !SurfaceClass_Floor) {
                ShowSevereError(Surface(DRink(Item).SurfacePtr).Name +
                                " is an ice rink floor and is defined as not a floor.  This is not allowed.  A rink must be a floor.");
                ErrorsFound = true;
            } else if (Surface(DRink(Item).SurfacePtr).Construction == 0) {
                ShowSevereError(Surface(DRink(Item).SurfacePtr).Name + " has an invalid construction");
                ErrorsFound = true;
            } else if (!Construct(Surface(DRink(Item).SurfacePtr).Construction).SourceSinkPresent) {
                ShowSevereError("Construction referenced in Direct Refrigeration System Surface does not have a source/sink present");
                ShowContinueError("Surface name= " + Surface(DRink(Item).SurfacePtr).Name +
                                  "  Construction name = " + Construct(Surface(DRink(Item).SurfacePtr).Construction).Name);
                ShowContinueError("Construction needs to be defined with a \"Construction:InternalSource\" object.");
                ErrorsFound = true;
            } else {
                DRink(Item).NumOfSurfaces = 1;
                DRink(Item).SurfacePtr = 1;
                DRink(Item).SurfaceName = (DRink(Item).NumOfSurfaces);
                DRink(Item).SurfaceFlowFrac.allocate(DRink(Item).NumOfSurfaces);
                DRink(Item).NumCircuits.allocate(DRink(Item).NumOfSurfaces);
                DRink(Item).SurfaceFlowFrac(1) = 1.0;
                DRink(Item).NumCircuits(1) = 0.0;
            }

            DRink(Item).TubeDiameter = Numbers(1);
            DRink(Item).TubeLength = Numbers(2);

            // Process the temperature control type
            if (UtilityRoutines::SameString(Alphas(5), RefrigOutletTemperature)) {
                DRink(Item).ControlType = BrineOutletTempControl;
            } else if (UtilityRoutines::SameString(Alphas(5), IceSurfaceTemperature)) {
                DRink(Item).ControlType = SurfaceTempControl;
            } else {
                ShowWarningError("Invalid " + cAlphaFields(5) + " =" + Alphas(5));
                ShowContinueError("Occurs in " + CurrentModuleObject + " = " + Alphas(1));
                ShowContinueError("Control reset to MAT control for this Hydronic Radiant System.");
                DRink(Item).ControlType = SurfaceTempControl;
            }

            // Cooling user input data
            DRink(Item).RefrigVolFlowMaxCool = Numbers(3);

            DRink(Item).ColdRefrigInNode = GetOnlySingleNode(
                Alphas(6), ErrorsFound, CurrentModuleObject, Alphas(1), NodeType_Unknown, NodeConnectionType_Inlet, 1, ObjectIsNotParent);

            DRink(Item).ColdRefrigOutNode = GetOnlySingleNode(
                Alphas(7), ErrorsFound, CurrentModuleObject, Alphas(1), NodeType_Unknown, NodeConnectionType_Inlet, 1, ObjectIsNotParent);

            if ((!lAlphaBlanks(6)) || (!lAlphaBlanks(7))) {
                TestCompSet(CurrentModuleObject, Alphas(1), Alphas(6), Alphas(7), "Chilled Refrigerant Nodes");
            }

            DRink(Item).ColdThrottleRange = Numbers(4);
            if (DRink(Item).ColdThrottleRange < MinThrottlingRange) {
                ShowWarningError("IndoorIceRink:DirectRefrigSystem: Cooling throttling range too small, reset to 0.5");
                ShowContinueError("Occurs in Refrigeration System=" + DRink(Item).Name);
                DRink(Item).ColdThrottleRange = MinThrottlingRange;
            }

            DRink(Item).ColdSetptSched = Alphas(8);
            DRink(Item).ColdSetptSchedPtr = GetScheduleIndex(Alphas(8));
            if ((DRink(Item).ColdSetptSchedPtr == 0) && (!lAlphaBlanks(8))) {
                ShowSevereError(cAlphaFields(8) + " not found: " + Alphas(8));
                ShowContinueError("Occurs in " + CurrentModuleObject + " = " + Alphas(1));
                ErrorsFound = true;
            }
            if (UtilityRoutines::SameString(Alphas(9), Off)) {
                DRink(Item).CondCtrlType = CondCtrlNone;
            } else if (UtilityRoutines::SameString(Alphas(9), SimpleOff)) {
                DRink(Item).CondCtrlType = CondCtrlSimpleOff;
            } else if (UtilityRoutines::SameString(Alphas(9), VariableOff)) {
                DRink(Item).CondCtrlType = CondCtrlVariedOff;
            } else {
                DRink(Item).CondCtrlType = CondCtrlSimpleOff;
            }

            DRink(Item).CondDewPtDeltaT = Numbers(5);

            if (UtilityRoutines::SameString(Alphas(10), OnePerSurf)) {
                DRink(Item).NumCircCalcMethod = OneCircuit;
            } else if (UtilityRoutines::SameString(Alphas(10), CalcFromLength)) {
                DRink(Item).NumCircCalcMethod = CalculateFromLength;
            } else {
                DRink(Item).NumCircCalcMethod = OneCircuit;
            }

            DRink(Item).CircLength = Numbers(6);

            if ((DRink(Item).RefrigVolFlowMaxCool == AutoSize) &&
                (lAlphaBlanks(6) || lAlphaBlanks(7) || lAlphaBlanks(8) || (DRink(Item).ColdRefrigInNode <= 0) ||
                 (DRink(Item).ColdRefrigOutNode <= 0) || (DRink(Item).ColdSetptSchedPtr == 0))) {
                ShowSevereError("Direct Refrigeration systems may not be autosized without specification of nodes or schedules");
                ShowContinueError("Occurs in " + CurrentModuleObject + " (cooling input) =" + Alphas(1));
                ErrorsFound = true;
            }
        }

        CurrentModuleObject = "IndoorIceRink:IndirectRefrigSystem";
        for (Item = 1; Item <= NumOfIndirectRefrigSys; ++Item) {

            inputProcessor->getObjectItem(CurrentModuleObject,
                                          Item,
                                          Alphas,
                                          NumAlphas,
                                          Numbers,
                                          NumNumbers,
                                          IOStatus,
                                          lNumericBlanks,
                                          lAlphaBlanks,
                                          cAlphaFields,
                                          cNumericFields);
        }

        Alphas.deallocate();
        Numbers.deallocate();
        cAlphaFields.deallocate();
        cNumericFields.deallocate();
        lAlphaBlanks.deallocate();
        lNumericBlanks.deallocate();
    }

    void InitIndoorIceRink(bool const FirstHVACIteration, // TRUE if 1st HVAC simulation of system timestep
                           int const SysNum,              // Index for the low temperature radiant system under consideration within the derived types
                           int const SystemType,          // Type of radiant system: hydronic, constant flow, or electric
                           bool &InitErrorsFound)
    {
        // Using/Aliasing
        using DataGlobals::AnyPlantInModel;
        using DataGlobals::BeginEnvrnFlag;
        using DataGlobals::NumOfZones;
        using DataPlant::PlantLoop;
        using DataPlant::TypeOf_LowTempRadiant_VarFlow;
        using DataSurfaces::SurfaceClass_Floor;
        using PlantUtilities::InitComponentNodes;
        using PlantUtilities::SetComponentFlowRate;
        using PlantUtilities::ScanPlantLoopsForObject;
        // SUBROUTINE PARAMETER DEFINITIONS:
        Real64 const ZeroTol(0.0000001); // Smallest non-zero value allowed
        static std::string const RoutineName("InitLowTempRadiantSystem");
        int LoopCounter;
        int SurfNum;
        int SurfNum2;

        static Array1D_bool MyEnvrnFlagDRink;
        static Array1D_bool MyEnvrnFlagIRink;
        static bool MyEnvrnFlagGeneral(true);
        static bool ZoneEquipmentListChecked(false); // True after the Zone Equipment List has been checked for items
        int Loop;
        int ZoneNum;                     // Intermediate variable for keeping track of the zone number
        static bool MyOneTimeFlag(true); // Initialization flag
        static Array1D_bool MyPlantScanFlagDRink;
        static Array1D_bool MyPlantScanFlagIRink;
        Real64 mdot; // local fluid mass flow rate
        Real64 rho;  // local fluid density
        bool errFlag;

        InitErrorsFound = false;

        if (MyOneTimeFlag) {
            MyEnvrnFlagDRink.allocate(NumOfDirectRefrigSys);
            MyEnvrnFlagIRink.allocate(NumOfIndirectRefrigSys);
            MyPlantScanFlagDRink.allocate(NumOfDirectRefrigSys);
            MyPlantScanFlagIRink.allocate(NumOfIndirectRefrigSys);
            MyPlantScanFlagDRink = true;
            MyPlantScanFlagIRink = true;
            MyEnvrnFlagDRink = true;
            MyEnvrnFlagIRink = true;
            MyOneTimeFlag = false;
        }

        if (FirstTimeInit) {
            ZeroSourceSumHATsurf.dimension(NumOfZones, 0.0);
            QRadSysSrcAvg.dimension(TotSurfaces, 0.0);
            LastQRadSysSrc.dimension(TotSurfaces, 0.0);
            LastSysTimeElapsed.dimension(TotSurfaces, 0.0);

            // Initialize total area for all refrigeration systems
            for (LoopCounter = 1; LoopCounter <= NumOfDirectRefrigSys; ++LoopCounter) {
                for (SurfNum = 1; SurfNum <= DRink(LoopCounter).NumOfSurfaces; ++SurfNum) {
                    if (Surface(DRink(LoopCounter).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                        SurfNum2 = DRink(SysNum).SurfacePtrArray(SurfNum);
                        DRink(LoopCounter).SurfaceArea += Surface(DRink(LoopCounter).SurfacePtrArray(SurfNum2)).Area;
                    }
                }
            }

            for (LoopCounter = 1; LoopCounter <= NumOfIndirectRefrigSys; ++LoopCounter) {
                for (SurfNum = 1; SurfNum <= IRink(LoopCounter).NumOfSurfaces; ++SurfNum) {
                    if (Surface(IRink(LoopCounter).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                        SurfNum2 = IRink(SysNum).SurfacePtrArray(SurfNum);
                        DRink(LoopCounter).SurfaceArea += Surface(IRink(LoopCounter).SurfacePtrArray(SurfNum2)).Area;
                    }
                }
            }

            FirstTimeInit = false;
        }

        if (SystemType == DirectSystem) {
            if (MyPlantScanFlagDRink(SysNum) && allocated(PlantLoop)) {
                errFlag = false;
                if (DRink(SysNum).ColdRefrigInNode > 0) {
                    ScanPlantLoopsForObject(DRink(SysNum).Name,
                                            TypeOf_LowTempRadiant_VarFlow,
                                            DRink(SysNum).CRefrigLoopNum,
                                            DRink(SysNum).CRefrigLoopSide,
                                            DRink(SysNum).CRefrigBranchNum,
                                            DRink(SysNum).CRefrigCompNum,
                                            errFlag,
                                            _,
                                            _,
                                            _,
                                            DRink(SysNum).ColdRefrigInNode,
                                            _);
                    if (errFlag) {
                        ShowFatalError("InitIndoorIceRink: Program terminated due to previous condition(s).");
                    }
                }
                MyPlantScanFlagDRink(SysNum) = false;
            } else if (MyPlantScanFlagDRink(SysNum) && !AnyPlantInModel) {
                MyPlantScanFlagDRink(SysNum) = false;
            }
        }

        if (SystemType == IndirectSystem) {
            if (MyPlantScanFlagIRink(SysNum) && allocated(PlantLoop)) {
                errFlag = false;
                if (IRink(SysNum).ColdRefrigInNode > 0) {
                    ScanPlantLoopsForObject(IRink(SysNum).Name,
                                            TypeOf_LowTempRadiant_VarFlow,
                                            IRink(SysNum).CRefrigLoopNum,
                                            IRink(SysNum).CRefrigLoopSide,
                                            IRink(SysNum).CRefrigBranchNum,
                                            IRink(SysNum).CRefrigCompNum,
                                            errFlag,
                                            _,
                                            _,
                                            _,
                                            IRink(SysNum).ColdRefrigInNode,
                                            _);
                    if (errFlag) {
                        ShowFatalError("InitIndoorIceRink: Program terminated due to previous condition(s).");
                    }
                }
                MyPlantScanFlagIRink(SysNum) = false;
            } else if (MyPlantScanFlagIRink(SysNum) && !AnyPlantInModel) {
                MyPlantScanFlagIRink(SysNum) = false;
            }
        }

        if (BeginEnvrnFlag && MyEnvrnFlagGeneral) {
            ZeroSourceSumHATsurf = 0.0;
            QRadSysSrcAvg = 0.0;
            LastQRadSysSrc = 0.0;
            LastSysTimeElapsed = 0.0;
            LastTimeStepSys = 0.0;
            MyEnvrnFlagGeneral = false;
        }
        if (!BeginEnvrnFlag) MyEnvrnFlagGeneral = true;

        if (SystemType == DirectSystem) {
            if (BeginEnvrnFlag && MyEnvrnFlagDRink(SysNum)) {

                DRink(SysNum).RefrigInletTemp = 0.0;
                DRink(SysNum).RefrigOutletTemp = 0.0;
                DRink(SysNum).RefrigMassFlowRate = 0.0;
                DRink(SysNum).CoolPower = 0.0;
                DRink(SysNum).CoolEnergy = 0.0;

                if (!MyPlantScanFlagDRink(SysNum)) {
                    if (DRink(SysNum).ColdRefrigInNode > 0) {
                        InitComponentNodes(0.0,
                                           DRink(SysNum).RefrigFlowMaxCool,
                                           DRink(SysNum).ColdRefrigInNode,
                                           DRink(SysNum).ColdRefrigOutNode,
                                           DRink(SysNum).CRefrigLoopNum,
                                           DRink(SysNum).CRefrigLoopSide,
                                           DRink(SysNum).CRefrigBranchNum,
                                           DRink(SysNum).CRefrigCompNum);
                    }
                }
                MyEnvrnFlagDRink(SysNum) = false;
            }
        }
        if (!BeginEnvrnFlag && SystemType == DirectSystem) MyEnvrnFlagDRink(SysNum) = true;

        if (SystemType == IndirectSystem) {
            if (BeginEnvrnFlag && MyEnvrnFlagIRink(SysNum)) {

                IRink(SysNum).RefrigInletTemp = 0.0;
                IRink(SysNum).RefrigOutletTemp = 0.0;
                IRink(SysNum).RefrigMassFlowRate = 0.0;
                IRink(SysNum).CoolPower = 0.0;
                IRink(SysNum).CoolEnergy = 0.0;

                if (!MyPlantScanFlagIRink(SysNum)) {
                    if (IRink(SysNum).ColdRefrigInNode > 0) {
                        InitComponentNodes(0.0,
                                           IRink(SysNum).RefrigFlowMaxCool,
                                           IRink(SysNum).ColdRefrigInNode,
                                           IRink(SysNum).ColdRefrigOutNode,
                                           IRink(SysNum).CRefrigLoopNum,
                                           IRink(SysNum).CRefrigLoopSide,
                                           IRink(SysNum).CRefrigBranchNum,
                                           IRink(SysNum).CRefrigCompNum);
                    }
                }
                MyEnvrnFlagIRink(SysNum) = false;
            }
        }
        if (!BeginEnvrnFlag && SystemType == IndirectSystem) MyEnvrnFlagIRink(SysNum) = true;

        if (BeginTimeStepFlag && FirstHVACIteration) {
            {
                auto const SELECT_CASE_var(SystemType);

                if (SELECT_CASE_var == DirectSystem) {
                    ZoneNum = DRink(SysNum).ZonePtr;
                    ZeroSourceSumHATsurf(ZoneNum) = SumHATsurf(ZoneNum); // Set this to figure what partial part of the load the radiant system meets
                    for (SurfNum = 1; SurfNum < DRink(SysNum).NumOfSurfaces; ++SurfNum) {
                        if (Surface(DRink(SysNum).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                            SurfNum2 = DRink(SysNum).SurfacePtrArray(SurfNum);
                        }
                    }
                    QRadSysSrcAvg(SurfNum2) = 0.0;      // Initialize this variable to zero (refrigeration system defaults to off)
                    LastQRadSysSrc(SurfNum2) = 0.0;     // At the start of a time step, reset to zero so average calculation can begin again
                    LastSysTimeElapsed(SurfNum2) = 0.0; // At the start of a time step, reset to zero so average calculation can begin again
                    LastTimeStepSys(SurfNum2) = 0.0;    // At the start of a time step, reset to zero so average calculation can begin again
                } else if (SystemType == IndirectSystem) {
                    ZoneNum = IRink(SysNum).ZonePtr;
                    ZeroSourceSumHATsurf(ZoneNum) = SumHATsurf(ZoneNum); // Set this to figure what partial part of the load the radiant system meets
                    for (SurfNum = 1; SurfNum < IRink(SysNum).NumOfSurfaces; ++SurfNum) {
                        if (Surface(IRink(SysNum).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                            SurfNum2 = IRink(SysNum).SurfacePtrArray(SurfNum);
                        }
                    }
                    QRadSysSrcAvg(SurfNum2) = 0.0;      // Initialize this variable to zero (refrigeration system defaults to off)
                    LastQRadSysSrc(SurfNum2) = 0.0;     // At the start of a time step, reset to zero so average calculation can begin again
                    LastSysTimeElapsed(SurfNum2) = 0.0; // At the start of a time step, reset to zero so average calculation can begin again
                    LastTimeStepSys(SurfNum2) = 0.0;    // At the start of a time step, reset to zero so average calculation can begin again
                } else {

                    ShowSevereError("Refrigeration system entered without specification of type: Direct or Indirect?");
                    ShowContinueError("Occurs in Refrigeration System=" + DRink(SysNum).Name);
                    ShowFatalError("Preceding condition causes termination.");
                }
            }
        }

        {
            auto const SELECT_CASE_var(SystemType);

            if (SELECT_CASE_var == DirectSystem) {

                // Initialize the appropriate node data
                mdot = 0.0;
                SetComponentFlowRate(mdot,
                                     DRink(SysNum).ColdRefrigInNode,
                                     DRink(SysNum).ColdRefrigOutNode,
                                     DRink(SysNum).CRefrigLoopNum,
                                     DRink(SysNum).CRefrigLoopSide,
                                     DRink(SysNum).CRefrigBranchNum,
                                     DRink(SysNum).CRefrigCompNum);
            } else if (SELECT_CASE_var == IndirectSystem) {
                // Initialize the appropriate node data
                mdot = 0.0;
                SetComponentFlowRate(mdot,
                                     IRink(SysNum).ColdRefrigInNode,
                                     IRink(SysNum).ColdRefrigOutNode,
                                     IRink(SysNum).CRefrigLoopNum,
                                     IRink(SysNum).CRefrigLoopSide,
                                     IRink(SysNum).CRefrigBranchNum,
                                     IRink(SysNum).CRefrigCompNum);
            }
        }

        OperatingMode = NotOperating;
    }

    Real64 IceRinkFreezing(int const SysNum,      // Index to refrigeration system
                           Real64 FloodWaterTemp) // Flood Water Temperature
    {
        // Using/Aliasing
        using FluidProperties::GetDensityGlycol;
        using FluidProperties::GetSpecificHeatGlycol;
        using ScheduleManager::GetCurrentScheduleValue;
        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        static std::string const RoutineName("IceRinkFreezing");
        Real64 SetPointTemp; // temperature "goal" for the radiant system [Celsius]
        int SystemType;      // Type of refrigeration system: Direct or Indirect
        Real64 Length;       // Length of ice rink
        Real64 Width;        // Width of ice rink
        Real64 Depth;        // Thickness of ice
        Real64 Volume;       // Volume of ice structure
        Real64 CpWater;      // Specific heat of water
        Real64 RhoWater;     // Density of water
        Real64 QFusion(333550.00);
        Real64 CpIce(2108.00);
        Real64 QFreezing;

        SystemType = RefrigSysTypes(SysNum).SystemType;

        if (SystemType == DirectSystem) {
            if (DRink(SysNum).ColdSetptSchedPtr > 0) {
                SetPointTemp = GetCurrentScheduleValue(DRink(SysNum).ColdSetptSchedPtr);
            }
            RhoWater = GetDensityGlycol(fluidNameWater, FloodWaterTemp, DRink(SysNum).GlycolIndex, RoutineName);
            CpWater = GetSpecificHeatGlycol(fluidNameWater, FloodWaterTemp, DRink(SysNum).GlycolIndex, RoutineName);
            Length = DRink(SysNum).LengthRink;
            Width = DRink(SysNum).WidthRink;
            Depth = DRink(SysNum).IceThickness;
            Volume = Length * Width * Depth;
        } else if (SystemType == IndirectSystem) {
            if (IRink(SysNum).ColdSetptSchedPtr > 0) {
                SetPointTemp = GetCurrentScheduleValue(IRink(SysNum).ColdSetptSchedPtr);
            }
            RhoWater = GetDensityGlycol(fluidNameWater, FloodWaterTemp, IRink(SysNum).GlycolIndex, RoutineName);
            CpWater = GetSpecificHeatGlycol(fluidNameWater, FloodWaterTemp, IRink(SysNum).GlycolIndex, RoutineName);
            Length = IRink(SysNum).LengthRink;
            Width = IRink(SysNum).WidthRink;
            Depth = IRink(SysNum).IceThickness;
            Volume = Length * Width * Depth;
        }

        QFreezing = 0.001 * RhoWater * Volume * ((CpWater * FloodWaterTemp) + (QFusion) - (CpIce * SetPointTemp));

        return (0.0);
    }

    Real64 IceRinkResurfacer(Real64 ResurfacerTank_capacity,  // Resurfacing machine tank capacity(m3)
                             Real64 ResurfacingHWTemperature, // Temperature of flood water(C)
                             Real64 IceSurfaceTemperature,    // Temperature of ice rink surface(C)
                             Real64 InitResurfWaterTemp,      // Initial temperature of resurfacing water(C)
                             int const ResurfacerIndex,       // Index to the resurfacer machine
                             int const SysNum)                // Index to the refrigeration system
    {
        static std::string const RoutineName("IceRinkResurfacer");
        using FluidProperties::GetDensityGlycol;
        using FluidProperties::GetSpecificHeatGlycol;

        int SystemType;       // Type of refrigeration system
        Real64 QResurfacing;  // Heat input(KJ) to the rink due to resurfacing events
        Real64 EHeatingWater; // Electric energy(KJ) required to heat water
        Real64 QHumidity;     // Heat input(J) to the ice rink due to humidity change during resurfacing events
        Real64 CpWater;
        // Real64 DeltaHWaterToIce;
        Real64 RhoWater;
        Real64 QFusion(333550.00);
        Real64 CpIce(2108.00);
        Real64 MolarMassWater(18.015);
        Real64 T_air_preResurfacing;
        Real64 T_air_postResurfacing;
        Real64 RH_air_preResurfacing;
        Real64 RH_air_postResurfacing;
        Real64 VolumeRink;
        Real64 DeltaT_ice;
        Real64 DeltaAH_ice;
        Real64 AH_preResurfacing;
        Real64 AH_postResurfacing;

        SystemType = RefrigSysTypes(SysNum).SystemType;
        RhoWater = GetDensityGlycol(fluidNameWater, ResurfacingHWTemperature, Resurfacer(ResurfacerIndex).GlycolIndex, RoutineName);
        CpWater = GetSpecificHeatGlycol(fluidNameWater, ResurfacingHWTemperature, Resurfacer(ResurfacerIndex).GlycolIndex, RoutineName);
        QResurfacing =
            0.001 * RhoWater * ResurfacerTank_capacity * ((CpWater * ResurfacingHWTemperature) + (QFusion) - (CpIce * IceSurfaceTemperature));
        EHeatingWater = 0.001 * ResurfacerTank_capacity * RhoWater * CpWater * (ResurfacingHWTemperature - InitResurfWaterTemp);

        T_air_preResurfacing = IceSurfaceTemperature;
        T_air_postResurfacing = ResurfacingHWTemperature;
        RH_air_preResurfacing = 0;
        RH_air_postResurfacing = 1;
        DeltaT_ice = abs(IceSurfaceTemperature - ResurfacingHWTemperature);
        if (SystemType == DirectSystem) {
            VolumeRink = DRink(SysNum).LengthRink * DRink(SysNum).WidthRink * DRink(SysNum).DepthRink;
        } else {
            VolumeRink = IRink(SysNum).LengthRink * IRink(SysNum).WidthRink * IRink(SysNum).DepthRink;
        }
        AH_preResurfacing = ((6.112 * exp((17.67 * T_air_preResurfacing) / (T_air_preResurfacing + 243.5)) * RH_air_preResurfacing * MolarMassWater) /
                             (100 * 0.08314 * (273.15 + T_air_preResurfacing))) *
                            (1 / RhoWater);
        AH_postResurfacing =
            ((6.112 * exp((17.67 * T_air_postResurfacing) / (T_air_postResurfacing + 243.5)) * RH_air_postResurfacing * MolarMassWater) /
             (100 * 0.08314 * (273.15 + T_air_postResurfacing))) *
            (1 / RhoWater);
        DeltaAH_ice = abs(AH_preResurfacing - AH_postResurfacing);
        QHumidity = DeltaAH_ice * VolumeRink * DeltaT_ice * CpWater;

        return (QHumidity + QResurfacing);
    }

    Real64 CalcDRinkHXEffectTerm(Real64 const Temperature,    // Temperature of refrigerant entering the radiant system, in C
                                 int const SysNum,            // Index to the refrigeration system
                                 Real64 const RefrigMassFlow, // Mass flow rate of refrigerant in direct refrigeration system, kg/s
                                 Real64 TubeLength,           // Total length of the piping used in the radiant system
                                 Real64 TubeDiameter)
    {
        // Using/Aliasing
        using DataGlobals::Pi;
        using DataPlant::PlantLoop;
        using FluidProperties::GetSpecificHeatGlycol;

        // Return Value
        Real64 CalcDRinkHXEffectTerm;

        // SUBROUTINE PARAMETER DEFINITIONS:
        Real64 const MaxLaminarRe(2300.0); // Maximum Reynolds number for laminar flow
        int const NumOfPropDivisions(11);
        Real64 const MaxExpPower(50.0); // Maximum power after which EXP argument would be zero for DP variables
        static Array1D<Real64> const Temps(NumOfPropDivisions, {-10.00, -9.00, -8.00, -7.00, -6.00, -5.00, -4.00, -3.00, -2.00, -1.00, 0.00});
        static Array1D<Real64> const Mu(
            NumOfPropDivisions,
            {0.0001903, 0.0001881, 0.000186, 0.0001839, 0.0001818, 0.0001798, 0.0001778, 0.0001759, 0.000174, 0.0001721, 0.0001702});

        static Array1D<Real64> const Conductivity(NumOfPropDivisions,
                                                  {0.5902, 0.5871, 0.584, 0.5809, 0.5778, 0.5747, 0.5717, 0.5686, 0.5655, 0.5625, 0.5594});
        static Array1D<Real64> const Pr(NumOfPropDivisions, {1.471, 1.464, 1.456, 1.449, 1.442, 1.436, 1.429, 1.423, 1.416, 1.41, 1.404});
        static Array1D<Real64> const Cp(NumOfPropDivisions,
                                        {4563.00, 4568.00, 4573.00, 4578.00, 4583.00, 4589.00, 4594.00, 4599.00, 4604.00, 4610.00, 4615.00});
        static std::string const RoutineName("CalcDRinkHXEffectTerm");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int Index;
        Real64 InterpFrac;
        Real64 NuD;
        Real64 ReD;
        Real64 NTU;
        Real64 Cpactual;
        Real64 Kactual;
        Real64 MUactual;
        Real64 PRactual;
        Real64 Eff; // HX effectiveness

        // First find out where we are in the range of temperatures
        Index = 1;
        while (Index <= NumOfPropDivisions) {
            if (Temperature < Temps(Index)) break; // DO loop
            ++Index;
        }

        // Initialize thermal properties of water
        if (Index == 1) {
            MUactual = Mu(Index);
            Kactual = Conductivity(Index);
            PRactual = Pr(Index);
            Cpactual = Cp(Index);
        } else if (Index > NumOfPropDivisions) {
            Index = NumOfPropDivisions;
            MUactual = Mu(Index);
            Kactual = Conductivity(Index);
            PRactual = Pr(Index);
            Cpactual = Cp(Index);
        } else {
            InterpFrac = (Temperature - Temps(Index - 1)) / (Temps(Index) - Temps(Index - 1));
            MUactual = Mu(Index - 1) + InterpFrac * (Mu(Index) - Mu(Index - 1));
            Kactual = Conductivity(Index - 1) + InterpFrac * (Conductivity(Index) - Conductivity(Index - 1));
            PRactual = Pr(Index - 1) + InterpFrac * (Pr(Index) - Pr(Index - 1));
            Cpactual = Cp(Index - 1) + InterpFrac * (Cp(Index) - Cp(Index - 1));
        }

        // Claculate the reynold's number
        ReD = 4.0 * RefrigMassFlow / (Pi * MUactual * TubeDiameter);

        if (ReD >= MaxLaminarRe) { // Turbulent flow --> use Colburn equation
            NuD = 0.023 * std::pow(ReD, 0.8) * std::pow(PRactual, 1.0 / 3.0);
        } else { // Laminar flow --> use constant surface temperature relation
            NuD = 3.66;
        }

        NTU = Pi * Kactual * NuD * TubeLength / (RefrigMassFlow * Cpactual);
        if (NTU > MaxExpPower) {
            Eff = 1.0;
            CalcDRinkHXEffectTerm = RefrigMassFlow * Cpactual;
        } else {
            Eff = 1.0 - std::exp(-NTU);
            CalcDRinkHXEffectTerm = Eff * RefrigMassFlow * Cpactual;
        }

        return CalcDRinkHXEffectTerm;
    }

    Real64 CalcIRinkHXEffectTerm(Real64 const Temperature,    // Temperature of the refrigerant entering the radiant system
                                 int const SysNum,            // Index to the refrigeration system
                                 Real64 const RefrigMassFlow, // Mass flow rate of refrigerant in direct refrigeration system, kg/s
                                 Real64 TubeLength,           // Total length of the piping used in the radiant system
                                 Real64 TubeDiameter,         // Inner diameter of the piping used in the radiant system
                                 int const RefrigType, // Refrigerant used in the radiant system: Ethylene Glycol(EG) or Cslcium Chloride(CaCl2)
                                 Real64 Concentration  // Concentration of the brine(refrigerant) in the radiant system (allowed range 10% to 30%)
    )
    {
        // Using/Aliasing
        using DataGlobals::Pi;
        using DataPlant::PlantLoop;
        using FluidProperties::GetSpecificHeatGlycol;

        // Return Value
        Real64 CalcIRinkHXEffectTerm;

        Real64 const MaxLaminarRe(2300.0); // Maximum Reynolds number for laminar flow
        int const NumOfTempDivisions(11);  // Number of Temperature divisions (i.e the number of temperature data points)
        Real64 const MaxExpPower(50.0);    // Maximum power after which EXP argument would be zero for DP variables

        // Number of Data points for brines ( Calcium Chloride and Ethylene Glycol solution
        static Array1D<Real64> const Temperatures(NumOfTempDivisions, {-10.00, -9.00, -8.00, -7.00, -6.00, -5.00, -4.00, -3.00, -2.00, -1.00, 0.00});

        // Properties of Calcium chloride  solution at 25% - 30% concentration
        // Conductivity (Watt/m-C)
        static Array1D<Real64> const ConductivityCacl2_C25(NumOfTempDivisions,
                                                           {0.5253, 0.5267, 0.5281, 0.5296, 0.531, 0.5324, 0.5338, 0.5352, 0.5366, 0.5381, 0.5395});
        static Array1D<Real64> const ConductivityCacl2_C26(NumOfTempDivisions,
                                                           {0.524, 0.5254, 0.5268, 0.5283, 0.5297, 0.5311, 0.5325, 0.5339, 0.5353, 0.5367, 0.5381});
        static Array1D<Real64> const ConductivityCacl2_C27(NumOfTempDivisions,
                                                           {0.5227, 0.5241, 0.5255, 0.5269, 0.5284, 0.5298, 0.5312, 0.5326, 0.534, 0.5354, 0.5368});
        static Array1D<Real64> const ConductivityCacl2_C28(NumOfTempDivisions,
                                                           {0.5214, 0.5228, 0.5242, 0.5256, 0.527, 0.5285, 0.5299, 0.5313, 0.5327, 0.5341, 0.5355});
        static Array1D<Real64> const ConductivityCacl2_C29(NumOfTempDivisions,
                                                           {0.5201, 0.5215, 0.5229, 0.5243, 0.5258, 0.5272, 0.5286, 0.53, 0.5314, 0.5328, 0.5342});
        static Array1D<Real64> const ConductivityCacl2_C30(NumOfTempDivisions,
                                                           {0.5189, 0.5203, 0.5217, 0.5231, 0.5245, 0.5259, 0.5273, 0.5287, 0.5301, 0.5315, 0.5329});
        // Viscousity
        static Array1D<Real64> const MuCacl2_C25(
            NumOfTempDivisions, {0.00553, 0.005353, 0.005184, 0.005023, 0.004869, 0.004722, 0.004582, 0.004447, 0.004319, 0.004197, 0.004079});
        static Array1D<Real64> const MuCacl2_C26(
            NumOfTempDivisions, {0.005854, 0.005665, 0.005485, 0.005314, 0.005151, 0.004995, 0.004847, 0.004705, 0.004569, 0.00444, 0.004316});
        static Array1D<Real64> const MuCacl2_C27(
            NumOfTempDivisions, {0.006217, 0.006015, 0.005823, 0.005641, 0.005467, 0.005301, 0.005143, 0.004992, 0.004848, 0.00471, 0.004579});
        static Array1D<Real64> const MuCacl2_C28(
            NumOfTempDivisions, {0.006627, 0.00641, 0.006204, 0.006007, 0.005821, 0.005643, 0.005474, 0.005313, 0.005159, 0.005012, 0.004872});
        static Array1D<Real64> const MuCacl2_C29(
            NumOfTempDivisions, {0.007093, 0.006858, 0.006635, 0.006423, 0.006221, 0.00603, 0.005848, 0.005674, 0.005509, 0.005351, 0.0052});
        static Array1D<Real64> const MuCacl2_C30(
            NumOfTempDivisions, {0.007627, 0.00737, 0.007127, 0.006896, 0.006677, 0.006469, 0.006272, 0.006084, 0.005905, 0.005734, 0.005572});
        // Prandlt Number
        static Array1D<Real64> const PrCacl2_C25(NumOfTempDivisions, {29.87, 28.87, 27.91, 27.00, 26.13, 25.31, 24.52, 23.76, 23.04, 22.35, 21.69});
        static Array1D<Real64> const PrCacl2_C26(NumOfTempDivisions, {31.35, 30.29, 29.28, 28.32, 27.41, 26.54, 25.71, 24.92, 24.16, 23.44, 22.75});
        static Array1D<Real64> const PrCacl2_C27(NumOfTempDivisions, {33.02, 31.90, 30.83, 29.82, 28.85, 27.93, 27.05, 26.22, 25.42, 24.66, 23.93});
        static Array1D<Real64> const PrCacl2_C28(NumOfTempDivisions, {34.93, 33.73, 32.59, 31.51, 30.48, 29.50, 28.57, 27.68, 26.83, 26.03, 25.26});
        static Array1D<Real64> const PrCacl2_C29(NumOfTempDivisions, {37.10, 35.81, 34.58, 33.42, 32.32, 31.27, 30.27, 29.33, 28.42, 27.56, 26.74});
        static Array1D<Real64> const PrCacl2_C30(NumOfTempDivisions, {39.59, 38.19, 36.86, 35.60, 34.41, 33.28, 32.20, 31.18, 30.21, 29.29, 28.41});
        // Specific heat
        static Array1D<Real64> const CpCacl2_C25(NumOfTempDivisions,
                                                 {2837.00, 2840.00, 2844.00, 2847.00, 2850.00, 2853.00, 2856.00, 2859.00, 2863.00, 2866.00, 2869.00});
        static Array1D<Real64> const CpCacl2_C26(NumOfTempDivisions,
                                                 {2806.00, 2809.00, 2812.00, 2815.00, 2819.00, 2822.00, 2825.00, 2828.00, 2831.00, 2834.00, 2837.00});
        static Array1D<Real64> const CpCacl2_C27(NumOfTempDivisions,
                                                 {2777.00, 2780.00, 2783.00, 2786.00, 2789.00, 2792.00, 2794.00, 2797.00, 2800.00, 2803.00, 2806.00});
        static Array1D<Real64> const CpCacl2_C28(NumOfTempDivisions,
                                                 {2748.00, 2751.00, 2754.00, 2757.00, 2760.00, 2762.00, 2765.00, 2768.00, 2771.00, 2774.00, 2776.00});
        static Array1D<Real64> const CpCacl2_C29(NumOfTempDivisions,
                                                 {2721.00, 2723.00, 2726.00, 2729.00, 2731.00, 2734.00, 2736.00, 2739.00, 2742.00, 2744.00, 2747.00});
        static Array1D<Real64> const CpCacl2_C30(NumOfTempDivisions,
                                                 {2693.00, 2696.00, 2698.00, 2700.00, 2703.00, 2705.00, 2708.00, 2710.00, 2712.00, 2715.00, 2717.00});

        // Properties of Ethylene Glycol solution at 10%, 20% and 30% concentration
        // Conductivity (Watt/m-C)
        static Array1D<Real64> const ConductivityEG_C25(NumOfTempDivisions,
                                                        {0.4538, 0.4549, 0.456, 0.4571, 0.4582, 0.4593, 0.4604, 0.4615, 0.4626, 0.4637, 0.4648});
        static Array1D<Real64> const ConductivityEG_C26(NumOfTempDivisions,
                                                        {0.4502, 0.4513, 0.4524, 0.4535, 0.4546, 0.4557, 0.4567, 0.4578, 0.4589, 0.4599, 0.461});
        static Array1D<Real64> const ConductivityEG_C27(NumOfTempDivisions,
                                                        {0.4467, 0.4478, 0.4488, 0.4499, 0.4509, 0.452, 0.453, 0.4541, 0.4551, 0.4562, 0.4572});
        static Array1D<Real64> const ConductivityEG_C28(NumOfTempDivisions,
                                                        {0.4432, 0.4442, 0.4452, 0.4463, 0.4473, 0.4483, 0.4493, 0.4504, 0.4514, 0.4524, 0.4534});
        static Array1D<Real64> const ConductivityEG_C29(NumOfTempDivisions,
                                                        {0.4397, 0.4407, 0.4417, 0.4427, 0.4437, 0.4447, 0.4457, 0.4467, 0.4477, 0.4487, 0.4497});
        static Array1D<Real64> const ConductivityEG_C30(NumOfTempDivisions,
                                                        {0.4362, 0.4371, 0.4381, 0.4391, 0.4401, 0.4411, 0.442, 0.443, 0.444, 0.445, 0.4459});
        // Viscousity
        static Array1D<Real64> const MuEG_C25(
            NumOfTempDivisions, {0.005531, 0.0053, 0.005082, 0.004876, 0.00468, 0.004494, 0.004318, 0.004151, 0.003992, 0.003841, 0.003698});
        static Array1D<Real64> const MuEG_C26(
            NumOfTempDivisions, {0.005713, 0.005474, 0.005248, 0.005033, 0.00483, 0.004637, 0.004454, 0.004281, 0.004116, 0.003959, 0.003811});
        static Array1D<Real64> const MuEG_C27(
            NumOfTempDivisions, {0.005902, 0.005654, 0.005418, 0.005195, 0.004984, 0.004784, 0.004594, 0.004414, 0.004244, 0.004081, 0.003927});
        static Array1D<Real64> const MuEG_C28(
            NumOfTempDivisions, {0.006098, 0.005839, 0.005595, 0.005363, 0.005144, 0.004936, 0.004739, 0.004552, 0.004375, 0.004207, 0.004047});
        static Array1D<Real64> const MuEG_C29(
            NumOfTempDivisions, {0.006299, 0.006031, 0.005776, 0.005536, 0.005308, 0.005093, 0.004888, 0.004694, 0.004511, 0.004336, 0.004171});
        static Array1D<Real64> const MuEG_C30(
            NumOfTempDivisions, {0.006508, 0.006228, 0.005964, 0.005715, 0.005478, 0.005254, 0.005042, 0.004841, 0.00465, 0.004469, 0.004298});
        // Prandlt Number
        static Array1D<Real64> const PrEG_C25(NumOfTempDivisions, {45.57, 43.59, 41.72, 39.95, 38.28, 36.70, 35.20, 33.77, 32.43, 31.15, 29.93});
        static Array1D<Real64> const PrEG_C26(NumOfTempDivisions, {47.17, 45.11, 43.17, 41.34, 39.60, 37.95, 36.40, 34.92, 33.52, 32.19, 30.94});
        static Array1D<Real64> const PrEG_C27(NumOfTempDivisions, {48.82, 46.69, 44.67, 42.76, 40.96, 39.25, 37.64, 36.10, 34.65, 33.27, 31.97});
        static Array1D<Real64> const PrEG_C28(NumOfTempDivisions, {50.53, 48.31, 46.22, 44.24, 42.36, 40.59, 38.91, 37.32, 35.81, 34.39, 33.03});
        static Array1D<Real64> const PrEG_C29(NumOfTempDivisions, {52.29, 49.99, 47.81, 45.76, 43.81, 41.97, 40.23, 38.58, 37.01, 35.53, 34.13});
        static Array1D<Real64> const PrEG_C30(NumOfTempDivisions, {54.12, 51.72, 49.46, 47.32, 45.30, 43.39, 41.58, 39.87, 38.25, 36.71, 35.25});
        // Specific heat
        static Array1D<Real64> const CpEG_C25(NumOfTempDivisions,
                                              {3739.00, 3741.00, 3744.00, 3746.00, 3748.00, 3751.00, 3753.00, 3756.00, 3758.00, 3760.00, 3763.00});
        static Array1D<Real64> const CpEG_C26(NumOfTempDivisions,
                                              {3717.00, 3719.00, 3722.00, 3725.00, 3727.00, 3730.00, 3732.00, 3735.00, 3737.00, 3740.00, 3742.00});
        static Array1D<Real64> const CpEG_C27(NumOfTempDivisions,
                                              {3695.00, 3698.00, 3700.00, 3703.00, 3706.00, 3708.00, 3711.00, 3714.00, 3716.00, 3719.00, 3722.00});
        static Array1D<Real64> const CpEG_C28(NumOfTempDivisions,
                                              {3672.00, 3675.00, 3678.00, 3681.00, 3684.00, 3687.00, 3689.00, 3692.00, 3695.00, 3698.00, 3701.00});
        static Array1D<Real64> const CpEG_C29(NumOfTempDivisions,
                                              {3650.00, 3653.00, 3656.00, 3659.00, 3662.00, 3665.00, 3668.00, 3671.00, 3674.00, 3677.00, 3680.00});
        static Array1D<Real64> const CpEG_C30(NumOfTempDivisions,
                                              {3627.00, 3630.00, 3633.00, 3636.00, 3640.00, 3643.00, 3646.00, 3649.00, 3652.00, 3655.00, 3658.00});

        static std::string const RoutineName("CalcIRinkHXEffectTerm");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int Index;
        Real64 InterpFrac;
        Real64 NuD;
        Real64 ReD;
        Real64 NTU;
        Real64 Kactual;
        Real64 MUactual;
        Real64 PRactual;
        Real64 Cpactual;
        Real64 Eff; // HX effectiveness

        // First find out where we are in the range of temperatures
        Index = 1;
        while (Index <= NumOfTempDivisions) {
            if (Temperature < Temperatures(Index)) break; // DO loop
            ++Index;
        }

        if (RefrigType == CaCl2) {
            // Initialize thermal properties of Calcium Chloride Solution
            if (Concentration == 25.00) {
                if (Index == 1) {
                    MUactual = MuCacl2_C25(Index);
                    Kactual = ConductivityCacl2_C25(Index);
                    PRactual = PrCacl2_C25(Index);
                    Cpactual = CpCacl2_C25(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuCacl2_C25(Index);
                    Kactual = ConductivityCacl2_C25(Index);
                    PRactual = PrCacl2_C25(Index);
                    Cpactual = CpCacl2_C25(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuCacl2_C25(Index - 1) + InterpFrac * (MuCacl2_C25(Index) - MuCacl2_C25(Index - 1));
                    Kactual = ConductivityCacl2_C25(Index - 1) + InterpFrac * (ConductivityCacl2_C25(Index) - ConductivityCacl2_C25(Index - 1));
                    PRactual = PrCacl2_C25(Index - 1) + InterpFrac * (PrCacl2_C25(Index) - PrCacl2_C25(Index - 1));
                    Cpactual = CpCacl2_C25(Index - 1) + InterpFrac * (CpCacl2_C25(Index) - CpCacl2_C25(Index - 1));
                }
            } else if (Concentration == 26.00) {
                if (Index == 1) {
                    MUactual = MuCacl2_C26(Index);
                    Kactual = ConductivityCacl2_C26(Index);
                    PRactual = PrCacl2_C26(Index);
                    Cpactual = CpCacl2_C26(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuCacl2_C26(Index);
                    Kactual = ConductivityCacl2_C26(Index);
                    PRactual = PrCacl2_C26(Index);
                    Cpactual = CpCacl2_C26(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuCacl2_C26(Index - 1) + InterpFrac * (MuCacl2_C26(Index) - MuCacl2_C26(Index - 1));
                    Kactual = ConductivityCacl2_C26(Index - 1) + InterpFrac * (ConductivityCacl2_C26(Index) - ConductivityCacl2_C26(Index - 1));
                    PRactual = PrCacl2_C26(Index - 1) + InterpFrac * (PrCacl2_C26(Index) - PrCacl2_C26(Index - 1));
                    Cpactual = CpCacl2_C26(Index - 1) + InterpFrac * (CpCacl2_C26(Index) - CpCacl2_C26(Index - 1));
                }

            } else if (Concentration = 27.00) {
                if (Index == 1) {
                    MUactual = MuCacl2_C27(Index);
                    Kactual = ConductivityCacl2_C27(Index);
                    PRactual = PrCacl2_C27(Index);
                    Cpactual = CpCacl2_C27(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuCacl2_C27(Index);
                    Kactual = ConductivityCacl2_C27(Index);
                    PRactual = PrCacl2_C27(Index);
                    Cpactual = CpCacl2_C27(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuCacl2_C27(Index - 1) + InterpFrac * (MuCacl2_C27(Index) - MuCacl2_C27(Index - 1));
                    Kactual = ConductivityCacl2_C27(Index - 1) + InterpFrac * (ConductivityCacl2_C27(Index) - ConductivityCacl2_C27(Index - 1));
                    PRactual = PrCacl2_C27(Index - 1) + InterpFrac * (PrCacl2_C27(Index) - PrCacl2_C27(Index - 1));
                    Cpactual = CpCacl2_C27(Index - 1) + InterpFrac * (CpCacl2_C27(Index) - CpCacl2_C27(Index - 1));
                }
            } else if (Concentration = 28.00) {
                if (Index == 1) {
                    MUactual = MuCacl2_C28(Index);
                    Kactual = ConductivityCacl2_C28(Index);
                    PRactual = PrCacl2_C28(Index);
                    Cpactual = CpCacl2_C28(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuCacl2_C28(Index);
                    Kactual = ConductivityCacl2_C28(Index);
                    PRactual = PrCacl2_C28(Index);
                    Cpactual = CpCacl2_C28(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuCacl2_C28(Index - 1) + InterpFrac * (MuCacl2_C28(Index) - MuCacl2_C28(Index - 1));
                    Kactual = ConductivityCacl2_C28(Index - 1) + InterpFrac * (ConductivityCacl2_C28(Index) - ConductivityCacl2_C28(Index - 1));
                    PRactual = PrCacl2_C28(Index - 1) + InterpFrac * (PrCacl2_C28(Index) - PrCacl2_C28(Index - 1));
                    Cpactual = CpCacl2_C28(Index - 1) + InterpFrac * (CpCacl2_C28(Index) - CpCacl2_C28(Index - 1));
                }
            } else if (Concentration = 29.00) {
                if (Index == 1) {
                    MUactual = MuCacl2_C29(Index);
                    Kactual = ConductivityCacl2_C29(Index);
                    PRactual = PrCacl2_C29(Index);
                    Cpactual = CpCacl2_C29(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuCacl2_C29(Index);
                    Kactual = ConductivityCacl2_C29(Index);
                    PRactual = PrCacl2_C29(Index);
                    Cpactual = CpCacl2_C29(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuCacl2_C29(Index - 1) + InterpFrac * (MuCacl2_C29(Index) - MuCacl2_C29(Index - 1));
                    Kactual = ConductivityCacl2_C29(Index - 1) + InterpFrac * (ConductivityCacl2_C29(Index) - ConductivityCacl2_C29(Index - 1));
                    PRactual = PrCacl2_C29(Index - 1) + InterpFrac * (PrCacl2_C29(Index) - PrCacl2_C29(Index - 1));
                    Cpactual = CpCacl2_C29(Index - 1) + InterpFrac * (CpCacl2_C29(Index) - CpCacl2_C29(Index - 1));
                }
            } else {
                if (Index == 1) {
                    MUactual = MuCacl2_C30(Index);
                    Kactual = ConductivityCacl2_C30(Index);
                    PRactual = PrCacl2_C30(Index);
                    Cpactual = CpCacl2_C30(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuCacl2_C30(Index);
                    Kactual = ConductivityCacl2_C30(Index);
                    PRactual = PrCacl2_C30(Index);
                    Cpactual = CpCacl2_C30(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuCacl2_C30(Index - 1) + InterpFrac * (MuCacl2_C30(Index) - MuCacl2_C30(Index - 1));
                    Kactual = ConductivityCacl2_C30(Index - 1) + InterpFrac * (ConductivityCacl2_C30(Index) - ConductivityCacl2_C30(Index - 1));
                    PRactual = PrCacl2_C30(Index - 1) + InterpFrac * (PrCacl2_C30(Index) - PrCacl2_C30(Index - 1));
                    Cpactual = CpCacl2_C30(Index - 1) + InterpFrac * (CpCacl2_C30(Index) - CpCacl2_C30(Index - 1));
                }
            }
            // Claculate the Reynold's number
            ReD = 4.0 * RefrigMassFlow / (Pi * MUactual * TubeDiameter);
            if (ReD >= MaxLaminarRe) { // Turbulent flow --> use Colburn equation
                NuD = 0.023 * std::pow(ReD, 0.8) * std::pow(PRactual, 1.0 / 3.0);
            } else { // Laminar flow --> use constant surface temperature relation
                NuD = 3.66;
            }
            NTU = Pi * Kactual * NuD * TubeLength / (RefrigMassFlow * Cpactual);
            if (NTU > MaxExpPower) {
                Eff = 1.0;
                CalcIRinkHXEffectTerm = RefrigMassFlow * Cpactual;

            } else {
                Eff = 1.0 - std::exp(-NTU);
                CalcIRinkHXEffectTerm = Eff * RefrigMassFlow * Cpactual;
            }

        } else if (RefrigType == EG) {
            // Initialize thermal properties of Ethylene Glycol Solution
            if (Concentration == 25.00) {
                if (Index == 1) {
                    MUactual = MuEG_C25(Index);
                    Kactual = ConductivityEG_C25(Index);
                    PRactual = PrEG_C25(Index);
                    Cpactual = CpEG_C25(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuEG_C25(Index);
                    Kactual = ConductivityEG_C25(Index);
                    PRactual = PrEG_C25(Index);
                    Cpactual = CpEG_C25(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuEG_C25(Index - 1) + InterpFrac * (MuEG_C25(Index) - MuEG_C25(Index - 1));
                    Kactual = ConductivityEG_C25(Index - 1) + InterpFrac * (ConductivityEG_C25(Index) - ConductivityEG_C25(Index - 1));
                    PRactual = PrEG_C25(Index - 1) + InterpFrac * (PrEG_C25(Index) - PrEG_C25(Index - 1));
                    Cpactual = CpEG_C25(Index - 1) + InterpFrac * (CpEG_C25(Index) - CpEG_C25(Index - 1));
                }
            } else if (Concentration == 26.00) {
                if (Index == 1) {
                    MUactual = MuEG_C26(Index);
                    Kactual = ConductivityEG_C26(Index);
                    PRactual = PrEG_C26(Index);
                    Cpactual = CpEG_C26(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuEG_C26(Index);
                    Kactual = ConductivityEG_C26(Index);
                    PRactual = PrEG_C26(Index);
                    Cpactual = CpEG_C26(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuEG_C26(Index - 1) + InterpFrac * (MuEG_C26(Index) - MuEG_C26(Index - 1));
                    Kactual = ConductivityEG_C26(Index - 1) + InterpFrac * (ConductivityEG_C26(Index) - ConductivityEG_C26(Index - 1));
                    PRactual = PrEG_C26(Index - 1) + InterpFrac * (PrEG_C26(Index) - PrEG_C26(Index - 1));
                    Cpactual = CpEG_C26(Index - 1) + InterpFrac * (CpEG_C26(Index) - CpEG_C26(Index - 1));
                }

            } else if (Concentration = 27.00) {
                if (Index == 1) {
                    MUactual = MuEG_C27(Index);
                    Kactual = ConductivityEG_C27(Index);
                    PRactual = PrEG_C27(Index);
                    Cpactual = CpEG_C27(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuEG_C27(Index);
                    Kactual = ConductivityEG_C27(Index);
                    PRactual = PrEG_C27(Index);
                    Cpactual = CpEG_C27(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuEG_C27(Index - 1) + InterpFrac * (MuEG_C27(Index) - MuEG_C27(Index - 1));
                    Kactual = ConductivityEG_C27(Index - 1) + InterpFrac * (ConductivityEG_C27(Index) - ConductivityEG_C27(Index - 1));
                    PRactual = PrEG_C27(Index - 1) + InterpFrac * (PrEG_C27(Index) - PrEG_C27(Index - 1));
                    Cpactual = CpEG_C27(Index - 1) + InterpFrac * (CpEG_C27(Index) - CpEG_C27(Index - 1));
                }
            } else if (Concentration = 28.00) {
                if (Index == 1) {
                    MUactual = MuEG_C28(Index);
                    Kactual = ConductivityEG_C28(Index);
                    PRactual = PrEG_C28(Index);
                    Cpactual = CpEG_C28(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuEG_C28(Index);
                    Kactual = ConductivityEG_C28(Index);
                    PRactual = PrEG_C28(Index);
                    Cpactual = CpEG_C28(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuEG_C28(Index - 1) + InterpFrac * (MuEG_C28(Index) - MuEG_C28(Index - 1));
                    Kactual = ConductivityEG_C28(Index - 1) + InterpFrac * (ConductivityEG_C28(Index) - ConductivityEG_C28(Index - 1));
                    PRactual = PrEG_C28(Index - 1) + InterpFrac * (PrEG_C28(Index) - PrEG_C28(Index - 1));
                    Cpactual = CpEG_C28(Index - 1) + InterpFrac * (CpEG_C28(Index) - CpEG_C28(Index - 1));
                }
            } else if (Concentration = 29.00) {
                if (Index == 1) {
                    MUactual = MuEG_C29(Index);
                    Kactual = ConductivityEG_C29(Index);
                    PRactual = PrEG_C29(Index);
                    Cpactual = CpEG_C29(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuEG_C29(Index);
                    Kactual = ConductivityEG_C29(Index);
                    PRactual = PrEG_C29(Index);
                    Cpactual = CpEG_C29(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuEG_C29(Index - 1) + InterpFrac * (MuEG_C29(Index) - MuEG_C29(Index - 1));
                    Kactual = ConductivityEG_C29(Index - 1) + InterpFrac * (ConductivityEG_C29(Index) - ConductivityEG_C29(Index - 1));
                    PRactual = PrEG_C29(Index - 1) + InterpFrac * (PrEG_C29(Index) - PrEG_C29(Index - 1));
                    Cpactual = CpEG_C29(Index - 1) + InterpFrac * (CpEG_C29(Index) - CpEG_C29(Index - 1));
                }
            } else {
                if (Index == 1) {
                    MUactual = MuEG_C30(Index);
                    Kactual = ConductivityEG_C30(Index);
                    PRactual = PrEG_C30(Index);
                    Cpactual = CpEG_C30(Index);
                } else if (Index > NumOfTempDivisions) {
                    Index = NumOfTempDivisions;
                    MUactual = MuEG_C30(Index);
                    Kactual = ConductivityEG_C30(Index);
                    PRactual = PrEG_C30(Index);
                    Cpactual = CpEG_C30(Index);
                } else {
                    InterpFrac = (Temperature - Temperatures(Index - 1)) / (Temperatures(Index) - Temperatures(Index - 1));
                    MUactual = MuEG_C30(Index - 1) + InterpFrac * (MuEG_C30(Index) - MuEG_C30(Index - 1));
                    Kactual = ConductivityEG_C30(Index - 1) + InterpFrac * (ConductivityEG_C30(Index) - ConductivityEG_C30(Index - 1));
                    PRactual = PrEG_C30(Index - 1) + InterpFrac * (PrEG_C30(Index) - PrEG_C30(Index - 1));
                    Cpactual = CpEG_C30(Index - 1) + InterpFrac * (CpEG_C30(Index) - CpEG_C30(Index - 1));
                }
            }
            // Claculate the Reynold's number
            ReD = 4.0 * RefrigMassFlow / (Pi * MUactual * TubeDiameter);
            if (ReD >= MaxLaminarRe) { // Turbulent flow --> use Colburn equation
                NuD = 0.023 * std::pow(ReD, 0.8) * std::pow(PRactual, 1.0 / 3.0);
            } else { // Laminar flow --> use constant surface temperature relation
                NuD = 3.66;
            }
            NTU = Pi * Kactual * NuD * TubeLength / (RefrigMassFlow * Cpactual);
            if (NTU > MaxExpPower) {
                Eff = 1.0;
                CalcIRinkHXEffectTerm = RefrigMassFlow * Cpactual;

            } else {
                Eff = 1.0 - std::exp(-NTU);
                CalcIRinkHXEffectTerm = Eff * RefrigMassFlow * Cpactual;
            }
        }
        return CalcIRinkHXEffectTerm;
    }

    void CalcDirectIndoorIceRinkComps(int const SysNum, // Index number for the indirect refrigeration system
                                      Real64 &LoadMet)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Punya Sloka Dash
        //       DATE WRITTEN   August 2019

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine solves the direct type refrigeration system based on
        // how much refrigerant is (and the conditions of the refrigerant) supplied
        // to the radiant system. (NOTE: The refrigerant in direct system is ammonia, NH3).

        // METHODOLOGY EMPLOYED:
        // Use heat exchanger formulas to obtain the heat source/sink for the radiant
        // system based on the inlet conditions and flow rate of refrigerant. Once that is
        // determined, recalculate the surface heat balances to reflect this heat
        // addition/subtraction.  The load met by the system is determined by the
        // difference between the convection from all surfaces in the zone when
        // there was no radiant system output and with a source/sink added.

        // Using/Aliasing
        using DataEnvironment::OutBaroPress;
        using DataHeatBalance::Zone;
        using DataHeatBalFanSys::CTFTsrcConstPart;
        using DataHeatBalFanSys::MAT;
        using DataHeatBalFanSys::RadSysTiHBConstCoef;
        using DataHeatBalFanSys::RadSysTiHBQsrcCoef;
        using DataHeatBalFanSys::RadSysTiHBToutCoef;
        using DataHeatBalFanSys::RadSysToHBConstCoef;
        using DataHeatBalFanSys::RadSysToHBQsrcCoef;
        using DataHeatBalFanSys::RadSysToHBTinCoef;
        using DataHeatBalFanSys::ZoneAirHumRat;
        using DataHeatBalSurface::TH;
        using DataLoopNode::Node;
        using DataSurfaces::HeatTransferModel_CTF;
        using DataSurfaces::SurfaceClass_Floor;
        using General::RoundSigDigits;
        using PlantUtilities::SetComponentFlowRate;
        using ScheduleManager::GetCurrentScheduleValue;

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 SetPointTemp; // Set point temperature of the ice rink
        Real64 PeopleGain;   // heat gain from people in pool (assumed to be all convective)
        int CondSurfNum;     // Surface number (in radiant array) of
        int RefrigNodeIn;    // Node number of the refrigerant entering the refrigeration system
        int RefrigNodeOut;   // Node number of the refrigerant exiting the refrigeration system
        int ZoneNum;         // Zone pointer for this refrigeration system
        int SurfNum;         // DO loop counter for the surfaces that comprise a particular refrigeration system
        int SurfNum2;        // Index to the floor in the surface derived type
        int SurfNumA;        // DO loop counter for the surfaces that comprise a particular refrigeration system
        // int SurfNumB;                // DO loop counter for the surfaces that comprise a particular refrigeration system
        int SurfNum2A; // Index to the floor in the surface derived type
        // int SurfNumC;                // Index to surface for condensation control
        int ConstrNum;             // Index for construction number in Construct derived type
        Real64 SysRefrigMassFlow;  // System level refrigerant mass flow rate (includes effect of zone multiplier)
        Real64 RefrigMassFlow;     // Refrigerant mass flow rate in the refrigeration system, kg/s
        Real64 RefrigTempIn;       // Temperature of the refrigerant entering the refrigeration system, in C
        Real64 RefrigTempOut;      // Temperature of the refrigerant exiting the refrigeration system, in C
        Real64 EpsMdotCp;          // Epsilon (heat exchanger terminology) times refrigerant mass flow rate times water specific heat
        Real64 DewPointTemp;       // Dew-point temperature based on the zone air conditions
        Real64 LowestRadSurfTemp;  // Lowest surface temperature of a radiant system (when condensation is a concern)
        Real64 FullRefrigMassFlow; // Original refrigerant mass flow rate before reducing the flow for condensation concerns
        Real64 PredictedCondTemp;  // Temperature at which condensation is predicted (includes user parameter)
        Real64 ZeroFlowSurfTemp;   // Temperature of radiant surface when flow is zero
        Real64 ReductionFrac;      // Fraction that the flow should be reduced to avoid condensation

        Real64 Ca; // Coefficients to relate the inlet refrigerant temperature to the heat source
        Real64 Cb;
        Real64 Cc;
        Real64 Cd;
        Real64 Ce;
        Real64 Cf;
        Real64 Cg;
        Real64 Ch;
        Real64 Ci;
        Real64 Cj;
        Real64 Ck;
        Real64 Cl;

        RefrigNodeIn = DRink(SysNum).ColdRefrigInNode;
        if (RefrigNodeIn == 0) {
            ShowSevereError("Illegal inlet node for the refrigerant in the direct system");
            ShowFatalError("Preceding condition causes termination");
        }

        if (DRink(SysNum).ColdSetptSchedPtr > 0) {
            SetPointTemp = GetCurrentScheduleValue(DRink(SysNum).ColdSetptSchedPtr);
        }

        // Pointing the SurfNum2 to the floor as the rink can only be the floor
        for (SurfNum = 1; SurfNum <= DRink(SysNum).NumOfSurfaces; ++SurfNum) {
            if (Surface(DRink(SysNum).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                SurfNum2 = DRink(SysNum).SurfacePtrArray(SurfNum);
            }
        }
        ZoneNum = DRink(SysNum).ZonePtr;
        RefrigMassFlow = Node(RefrigNodeIn).MassFlowRate;
        RefrigTempIn = Node(RefrigNodeIn).Temp;

        // Heat gain from people (assumed to be all convective to pool water)
        if (DRink(SysNum).PeopleSchedPtr > 0) {
            DRink(SysNum).PeopleHeatGain = GetCurrentScheduleValue(DRink(SysNum).PeopleSchedPtr);
            PeopleGain = DRink(SysNum).PeopleHeatGain * DRink(SysNum).SpectatorArea;
        }

        if (RefrigMassFlow <= 0) {
            // No flow or below minimum allowed so there is no heat source/sink
            // This is possible with a mismatch between system and plant operation
            // or a slight mismatch between zone and system controls.  This is not
            // necessarily a "problem" so this exception is necessary in the code.

            QRadSysSource(SurfNum2) = 0.0;
        } else { // Refrigerant mass flow rate is significant
                 // Determine the heat exchanger "effectiveness" term
            EpsMdotCp = CalcDRinkHXEffectTerm(RefrigTempIn, SysNum, RefrigMassFlow, DRink(SysNum).TubeLength, DRink(SysNum).TubeLength);

            ConstrNum = Surface(SurfNum2).Construction;
            if (Surface(SurfNum2).HeatTransferAlgorithm == HeatTransferModel_CTF) {

                Ca = RadSysTiHBConstCoef(SurfNum2);
                Cb = RadSysTiHBToutCoef(SurfNum2);
                Cc = RadSysTiHBQsrcCoef(SurfNum2);

                Cd = RadSysToHBConstCoef(SurfNum2);
                Ce = RadSysToHBTinCoef(SurfNum2);
                Cf = RadSysToHBQsrcCoef(SurfNum2);

                Cg = CTFTsrcConstPart(SurfNum2);
                Ch = Construct(ConstrNum).CTFTSourceQ(0);
                Ci = Construct(ConstrNum).CTFTSourceIn(0);
                Cj = Construct(ConstrNum).CTFTSourceOut(0);

                Ck = Cg + ((Ci * (Ca + Cb * Cd) + Cj * (Cd + Ce * Ca)) / (1.0 - Ce * Cb));
                Cl = Ch + ((Ci * (Cc + Cb * Cf) + Cj * (Cf + Ce * Cc)) / (1.0 - Ce * Cb));

                QRadSysSource(SurfNum2) = EpsMdotCp * (RefrigTempIn - Ck) / (1.0 + (EpsMdotCp * Cl / Surface(SurfNum2).Area));
            }

            // "Temperature Comparision" Cut-off:
            // Check to see whether or not the system should really be running.  If
            // QRadSysSource is positive i.e. the system is giving heat to the rink,
            // then the radiant system will be doing the opposite of its intention.
            // In this case, the flow rate is set to zero to avoid heating.

            if ((OperatingMode == CoolingMode) && (QRadSysSource(SurfNum) >= 0.0)) {
                RefrigMassFlow = 0.0;
                SetComponentFlowRate(RefrigMassFlow,
                                     DRink(SysNum).ColdRefrigInNode,
                                     DRink(SysNum).ColdRefrigOutNode,
                                     DRink(SysNum).CRefrigLoopNum,
                                     DRink(SysNum).CRefrigLoopSide,
                                     DRink(SysNum).CRefrigBranchNum,
                                     DRink(SysNum).CRefrigCompNum);

                DRink(SysNum).RefrigMassFlowRate = RefrigMassFlow;
            }
        }

        LoadMet = SumHATsurf(ZoneNum) - ZeroSourceSumHATsurf(ZoneNum) + PeopleGain;
    }

    void CalcDirectIndoorIceRinkSys(int const SysNum, // name of the direct refrigeration system
                                    Real64 &LoadMet   // load met by the direct refrigeration system, in Watts
    )
    {
        // Using/Aliasing
        using DataSurfaces::SurfaceClass_Floor;
        using PlantUtilities::SetComponentFlowRate;
        using ScheduleManager::GetCurrentScheduleValue;

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int ControlNode;  // the cold brine inlet node
        int ZoneNum;      // number of zone being served
        int SurfNum;      // Surface number in the Surface derived type for a rink surface
        int SurfNum2;     // Surface number in the Surface derived type for a rink surface
        Real64 mdot;      // local temporary for fluid mass flow rate
        Real64 RefInTemp; // Refrigerant inlet temperature

        ControlNode = 0;
        ZoneNum = DRink(SysNum).ZonePtr;
        OperatingMode = NotOperating;
        RefInTemp = DRink(SysNum).RefrigInletTemp;

        // Pointing the SurfNum2 to the floor as the rink can only be the floor
        for (SurfNum = 1; SurfNum <= DRink(SysNum).NumOfSurfaces; ++SurfNum) {
            if (Surface(DRink(SysNum).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                SurfNum2 = DRink(SysNum).SurfacePtrArray(SurfNum);
            }
        }

        if (GetCurrentScheduleValue(DRink(SysNum).SchedPtr) <= 0) {
            QRadSysSource(SurfNum2) = 0.0;

            SetComponentFlowRate(mdot,
                                 DRink(SysNum).ColdRefrigInNode,
                                 DRink(SysNum).ColdRefrigOutNode,
                                 DRink(SysNum).CRefrigLoopNum,
                                 DRink(SysNum).CRefrigLoopSide,
                                 DRink(SysNum).CRefrigBranchNum,
                                 DRink(SysNum).CRefrigCompNum);
        } else { // Unit might be on-->this section is intended to control the brine mass flow rate being
            // sent to the radiant system

            // Set the required mass flow rate for the refrigeration system
            {
                auto const SELECT_CASE_var(DRink(SysNum).ControlType);
                if (SELECT_CASE_var == BrineOutletTempControl) {
                    mdot = BOTC(DirectSystem, SysNum, RefInTemp);
                } else if (SELECT_CASE_var == SurfaceTempControl) {
                    mdot = STC();
                } else { // Should never get here
                    mdot = STC();
                    ShowSevereError("Illegal control type in direct refrigeration system system: " + DRink(SysNum).Name);
                    ShowFatalError("Preceding condition causes termination.");
                }
            }

            CalcDirectIndoorIceRinkComps(SysNum, LoadMet);
        }
    }

    Real64 SumHATsurf(int const ZoneNum) // Zone number
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Peter Graham Ellis
        //       DATE WRITTEN   July 2003

        // PURPOSE OF THIS FUNCTION:
        // This function calculates the zone sum of Hc*Area*Tsurf.  It replaces the old SUMHAT.
        // The SumHATsurf code below is also in the CalcZoneSums subroutine in ZoneTempPredictorCorrector
        // and should be updated accordingly.

        // Using/Aliasing
        using namespace DataSurfaces;
        using namespace DataHeatBalance;
        using namespace DataHeatBalSurface;

        // Return value
        Real64 SumHATsurf;

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int SurfNum; // Surface number
        Real64 Area; // Effective surface area

        SumHATsurf = 0.0;

        for (SurfNum = Zone(ZoneNum).SurfaceFirst; SurfNum <= Zone(ZoneNum).SurfaceLast; ++SurfNum) {
            if (!Surface(SurfNum).HeatTransSurf) continue; // Skip non-heat transfer surfaces

            Area = Surface(SurfNum).Area;

            if (Surface(SurfNum).Class == SurfaceClass_Window) {
                if (SurfaceWindow(SurfNum).ShadingFlag == IntShadeOn || SurfaceWindow(SurfNum).ShadingFlag == IntBlindOn) {
                    // The area is the shade or blind are = sum of the glazing area and the divider area (which is zero if no divider)
                    Area += SurfaceWindow(SurfNum).DividerArea;
                }

                if (SurfaceWindow(SurfNum).FrameArea > 0.0) {
                    // Window frame contribution
                    SumHATsurf += HConvIn(SurfNum) * SurfaceWindow(SurfNum).FrameArea * (1.0 + SurfaceWindow(SurfNum).ProjCorrFrIn) *
                                  SurfaceWindow(SurfNum).FrameTempSurfIn;
                }

                if (SurfaceWindow(SurfNum).DividerArea > 0.0 && SurfaceWindow(SurfNum).ShadingFlag != IntShadeOn &&
                    SurfaceWindow(SurfNum).ShadingFlag != IntBlindOn) {
                    // Window divider contribution (only from shade or blind for window with divider and interior shade or blind)
                    SumHATsurf += HConvIn(SurfNum) * SurfaceWindow(SurfNum).DividerArea * (1.0 + 2.0 * SurfaceWindow(SurfNum).ProjCorrDivIn) *
                                  SurfaceWindow(SurfNum).DividerTempSurfIn;
                }
            }

            SumHATsurf += HConvIn(SurfNum) * Area * TempSurfInTmp(SurfNum);
        }

        return SumHATsurf;
    }

    Real64 BOTC(int SystemType,     // Type of refrigeration system
                int SysNum,         // Index to the refrigeration system
                Real64 Temperature) // Temperature of inlet refrigerant
    {
        // Using/ Aliasing
        using DataGlobals::Pi;
        using DataHeatBalFanSys::CTFTsrcConstPart;
        using DataHeatBalFanSys::RadSysTiHBConstCoef;
        using DataHeatBalFanSys::RadSysTiHBQsrcCoef;
        using DataHeatBalFanSys::RadSysTiHBToutCoef;
        using DataHeatBalFanSys::RadSysToHBConstCoef;
        using DataHeatBalFanSys::RadSysToHBQsrcCoef;
        using DataHeatBalFanSys::RadSysToHBTinCoef;
        using DataLoopNode::Node;
        using DataSurfaces::SurfaceClass_Floor;
        using FluidProperties::GetSatSpecificHeatRefrig;
        using FluidProperties::GetSpecificHeatGlycol;
        using ScheduleManager::GetScheduleIndex;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("BrineOutletTemperatureControl");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int SurfNum;                    // Index to the surface number
        int SurfNum2;                   // Index to the surface number having source / sink(Floor)
        int ConstrNum;                  // Index for construction number in Construct derived type
        Real64 RefrigInTemp;            // Refrigerant inlet temperature
        Real64 RefrigOutTemp;           // Refrigerant outlet temperature
        Real64 RefrigOutTempDesired;    // Refrigerant outlet temperature which is desired,
                                        // so that BOTC can be established (to be obtained from the set point temperature)
        int RefrigNodeIn;               // Inlet node of refrigerant
        Real64 RefrigMassFlow;          // Initial guess value for mass flow rate of refrigerant
        Real64 RefrigMassFlow_Req(0.0); // Required mass flow rate of refrigerant to satisfy the BOTC
        Real64 QSource;                 // Heat flux from the heat source/sink
        Real64 PipeArea;                // Total area of the pipe used in the ice rink
        Real64 UA;                      // UA value of the piping

        Real64 Ca; // Coefficients to relate the inlet water temperature to the heat source
        Real64 Cb;
        Real64 Cc;
        Real64 Cd;
        Real64 Ce;
        Real64 Cf;
        Real64 Cg;
        Real64 Ch;
        Real64 Ci;
        Real64 Cj;
        Real64 Ck;
        Real64 Cl;

        Real64 const MaxLaminarRe(2300.0); // Maximum Reynolds number for laminar flow
        int const NumOfPropDivisions(11);
        Real64 const MaxExpPower(50.0); // Maximum power after which EXP argument would be zero for DP variables
        static Array1D<Real64> const Temps(NumOfPropDivisions, {-10.00, -9.00, -8.00, -7.00, -6.00, -5.00, -4.00, -3.00, -2.00, -1.00, 0.00});
        static Array1D<Real64> const Mu(
            NumOfPropDivisions,
            {0.0001903, 0.0001881, 0.000186, 0.0001839, 0.0001818, 0.0001798, 0.0001778, 0.0001759, 0.000174, 0.0001721, 0.0001702});

        static Array1D<Real64> const Conductivity(NumOfPropDivisions,
                                                  {0.5902, 0.5871, 0.584, 0.5809, 0.5778, 0.5747, 0.5717, 0.5686, 0.5655, 0.5625, 0.5594});
        static Array1D<Real64> const Pr(NumOfPropDivisions, {1.471, 1.464, 1.456, 1.449, 1.442, 1.436, 1.429, 1.423, 1.416, 1.41, 1.404});
        static Array1D<Real64> const Cp(NumOfPropDivisions,
                                        {4563.00, 4568.00, 4573.00, 4578.00, 4583.00, 4589.00, 4594.00, 4599.00, 4604.00, 4610.00, 4615.00});

        int Index;
        Real64 InterpFrac;
        Real64 NuD;
        Real64 ReD;
        Real64 NTU;
        Real64 Cpactual;
        Real64 Kactual;
        Real64 MUactual;
        Real64 PRactual;
        Real64 Eff;

        // First find out where we are in the range of temperatures
        Index = 1;
        while (Index <= NumOfPropDivisions) {
            if (Temperature < Temps(Index)) break; // DO loop
            ++Index;
        }

        // Initialize thermal properties of Ammonia
        if (Index == 1) {
            MUactual = Mu(Index);
            Kactual = Conductivity(Index);
            PRactual = Pr(Index);
            Cpactual = Cp(Index);
        } else if (Index > NumOfPropDivisions) {
            Index = NumOfPropDivisions;
            MUactual = Mu(Index);
            Kactual = Conductivity(Index);
            PRactual = Pr(Index);
            Cpactual = Cp(Index);
        } else {
            InterpFrac = (Temperature - Temps(Index - 1)) / (Temps(Index) - Temps(Index - 1));
            MUactual = Mu(Index - 1) + InterpFrac * (Mu(Index) - Mu(Index - 1));
            Kactual = Conductivity(Index - 1) + InterpFrac * (Conductivity(Index) - Conductivity(Index - 1));
            PRactual = Pr(Index - 1) + InterpFrac * (Pr(Index) - Pr(Index - 1));
            Cpactual = Cp(Index - 1) + InterpFrac * (Cp(Index) - Cp(Index - 1));
        }

        // Claculate the reynold's number
        ReD = 4.0 * RefrigMassFlow / (Pi * MUactual * DRink(SysNum).TubeDiameter);

        if (ReD >= MaxLaminarRe) { // Turbulent flow --> use Colburn equation
            NuD = 0.023 * std::pow(ReD, 0.8) * std::pow(PRactual, 1.0 / 3.0);
            UA = Pi * Kactual * NuD;
        } else { // Laminar flow --> use constant surface temperature relation
            NuD = 3.66;
            UA = Pi * Kactual * NuD;
        }

        if (SystemType == DirectSystem) {
            PipeArea = Pi * DRink(SysNum).TubeDiameter * DRink(SysNum).TubeLength;
            RefrigOutTempDesired = DRink(SysNum).RefOutBOTCCtrlTemp;
            RefrigMassFlow = 20.0;
            RefrigInTemp = Temperature;

            for (SurfNum = 1; SurfNum <= DRink(SysNum).NumOfSurfaces; ++SurfNum) {
                if (Surface(DRink(SysNum).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                    SurfNum2 = DRink(SysNum).SurfacePtrArray(SurfNum);
                }
            }
            ConstrNum = Surface(SurfNum2).Construction;

            Ca = RadSysTiHBConstCoef(SurfNum2);
            Cb = RadSysTiHBToutCoef(SurfNum2);
            Cc = RadSysTiHBQsrcCoef(SurfNum2);

            Cd = RadSysToHBConstCoef(SurfNum2);
            Ce = RadSysToHBTinCoef(SurfNum2);
            Cf = RadSysToHBQsrcCoef(SurfNum2);

            Cg = CTFTsrcConstPart(SurfNum2);
            Ch = Construct(ConstrNum).CTFTSourceQ(0);
            Ci = Construct(ConstrNum).CTFTSourceIn(0);
            Cj = Construct(ConstrNum).CTFTSourceOut(0);

            Ck = Cg + ((Ci * (Ca + Cb * Cd) + Cj * (Cd + Ce * Ca)) / (1.0 - Ce * Cb));
            Cl = Ch + ((Ci * (Cc + Cb * Cf) + Cj * (Cf + Ce * Cc)) / (1.0 - Ce * Cb));

            Term = exp(UA / (TempMDot * Cpactual)) - 1;
            Nr = Term * (Ck - RefrigInTemp);
            Dr = (Term * TempMDot * Cpactual * Cl) + ((Surface(DRink(SysNum).SurfacePtrArray(SurfNum2)).Area) * (Term + 1));
            RefrigOutTemp = RefrigInTemp + (Nr / Dr);

        } else if (SystemType == IndirectSystem) {
            PipeArea = Pi * IRink(SysNum).TubeDiameter * IRink(SysNum).TubeLength;
            RefrigOutTempDesired = IRink(SysNum).RefOutBOTCtrlTemp;
            RefrigMassFlow = 0.1;
            RefrigNodeIn = IRink(SysNum).ColdRefrigInNode;
            RefrigInTemp = Node(RefrigNodeIn).Temp;
            CpRef = GetSpecificHeatGlycol(IRink(SysNum).RefrigerantName, RefrigInTemp, IRink(SysNum).RefIndex, RoutineName);

            Eff = CalcIRinkHXEffectTerm(RefrigInTemp,
                                        SysNum,
                                        RefrigMassFlow,
                                        IRink(SysNum).TubeLength,
                                        IRink(SysNum).TubeDiameter,
                                        IRink(SysNum).RefrigType,
                                        IRink(SysNum).Concentration) /
                  (RefrigMassFlow * CpRef);

            for (SurfNum = 1; SurfNum <= IRink(SysNum).NumOfSurfaces; ++SurfNum) {
                if (Surface(IRink(SysNum).SurfacePtrArray(SurfNum)).Class == SurfaceClass_Floor) {
                    ConstrNum = Surface(SurfNum).Construction;

                    Ca = RadSysTiHBConstCoef(SurfNum);
                    Cb = RadSysTiHBToutCoef(SurfNum);
                    Cc = RadSysTiHBQsrcCoef(SurfNum);

                    Cd = RadSysToHBConstCoef(SurfNum);
                    Ce = RadSysToHBTinCoef(SurfNum);
                    Cf = RadSysToHBQsrcCoef(SurfNum);

                    Cg = CTFTsrcConstPart(SurfNum);
                    Ch = Construct(ConstrNum).CTFTSourceQ(0);
                    Ci = Construct(ConstrNum).CTFTSourceIn(0);
                    Cj = Construct(ConstrNum).CTFTSourceOut(0);

                    Ck = Cg + ((Ci * (Ca + Cb * Cd) + Cj * (Cd + Ce * Ca)) / (1.0 - Ce * Cb));
                    Cl = Ch + ((Ci * (Cc + Cb * Cf) + Cj * (Cf + Ce * Cc)) / (1.0 - Ce * Cb));

                    QSource = (RefrigInTemp - Ck) / ((Cl / Surface(SurfNum).Area) + (1 / (RefrigMassFlow * CpRef)));

                    RefrigOutTemp = RefrigInTemp - (QSource / (RefrigMassFlow * CpRef));

                    if (RefrigOutTemp <= RefrigOutTempDesired) { // Cooling is not required and refrigeration system should be off

                        RefrigOutTemp = RefrigOutTempDesired;
                        RefrigMassFlow_Req = IRink(SysNum).RefrigFlowMinCool;

                    } else if (RefrigOutTemp > RefrigOutTempDesired) { // Cooling is required and refrigeration system should be on

                        RefrigMassFlow_Req = (((Ck - RefrigInTemp) / (RefrigOutTemp - RefrigInTemp)) - (1 / Eff)) * ((PipeArea) / (CpRef * Cl));

                        if (RefrigMassFlow_Req >= IRink(SysNum).RefrigFlowMaxCool) { // The refrigeration system is undersized
                            RefrigMassFlow_Req = IRink(SysNum).RefrigFlowMaxCool;
                        }
                    }
                }
            }
        }

        return RefrigMassFlow_Req;
    }

    Real64 STC(int const SystemType, int const SysNum)
    {
        return (0.0);
    }

} // namespace IceRink
} // namespace EnergyPlus
