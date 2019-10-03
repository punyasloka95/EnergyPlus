#ifndef IceRink_hh_INCLUDED
#define IceRink_hh_INCLUDED

// ObjexxFCL Headers
#include <ObjexxFCL/Array1D.hh>

// EnergyPlus Headers
#include <DataGlobals.hh>
#include <EnergyPlus.hh>

namespace EnergyPlus {

namespace IceRink {

    // Data
    // MODULE PARAMETER DEFINITIONS:
    // System types:
    extern int const DirectSystem;
    extern int const IndirectSystem;
    extern std::string const cDRink;
    extern std::string const cIRink;

    // Fluid types in indirect refrigeration system
    extern int const CaCl2;
    extern int const EG;

    // Control types:
    extern int const SurfaceTempControl;
    extern int const BrineOutletTempControl;
    extern Real64 HighTempCooling;

    // Operating Mode:
    extern int NotOperating; // Parameter for use with OperatingMode variable, set for not operating
    extern int CoolingMode;  // Parameter for use with OperatingMode variable, set for cooling

    // Condensation control types:
    extern int const CondCtrlNone;
    extern int const CondCtrlSimpleOff;
    extern int const CondCtrlVariedOff;

    // Number of Circuits per Surface Calculation Method
    extern int const OneCircuit;
    extern int const CalculateFromLength;
    extern std::string const OnePerSurf;
    extern std::string const CalcFromLength;

    // DERIVED TYPE DEFINITIONS:

    // MODULE VARIABLE DECLARATIONS:
    // Standard, run-of-the-mill variables...
    extern int NumOfDirectRefrigSys;
    extern int NumOfIndirectRefrigSys;
    extern int TotalNumRefrigSystem;
    extern int OperatingMode;                    // Used to keep track of whether system is in heating or cooling mode
    extern Array1D<Real64> QRadSysSrcAvg;        // Average source over the time step for a particular radiant surface
    extern Array1D<Real64> ZeroSourceSumHATsurf; // Equal to SumHATsurf for all the walls in a zone with no source
    extern Array1D_bool CheckEquipName;

    struct DirectRefrigSysData
    {
        // Members
        // Input data
        std::string Name;                // name of direct refrigeration system
        std::string RefrigerantName;     // Name of refrigerant, must match name in FluidName
                                         //    (see fluidpropertiesrefdata.idf)
        int RefIndex;                    // Index number of refrigerant, automatically assigned on first call to fluid property
                                         //   and used thereafter
        std::string SchedName;           // availability schedule
        int SchedPtr;                    // index to schedule
        std::string ZoneName;            // Name of zone the system is serving
        int ZonePtr;                     // Point to this zone in the Zone derived type
        std::string SurfaceName;         // surface name of rink floor
        int SurfacePtr;                  // index to a surface
        Array1D_int SurfacePtrArray;     // index to a surface array
        int NumOfSurfaces;               // Number of surfaces included in this refrigeration system (coordinated control)
        Array1D<Real64> SurfaceFlowFrac; // Fraction of flow/pipe length for the floor surface
        Array1D<Real64> NumCircuits;     // Number of fluid circuits in the surface
        Real64 TubeDiameter;             // tube diameter for embedded tubing
        Real64 TubeLength;               // tube length embedded in radiant surface
        int ControlType;                 // Control type for the system(BOTC or STC)
        Real64 RefrigVolFlowMaxCool;     // maximum refrigerant flow rate for cooling, m3/s
        Real64 RefrigFlowMaxCool;        // maximum refrigerant mass flow rate for cooling. Kg/s
        Real64 RefrigFlowMinCool;        // manimum refrigerant mass flow rate for cooling. Kg/s
        int ColdRefrigInNode;            // cold refrigerant inlet node
        int ColdRefrigOutNode;           // cold refrigerant Outlet node
        Real64 ColdThrottleRange;        // Throttling range for cooling [C]
        std::string ColdSetptSched;      // Schedule name for the ice rink setpoint temperature
        int ColdSetptSchedPtr;           // Schedule index for the ice rink setpoint temperature
        Real64 CondDewPtDeltaT;          // Diff between surface temperature and dew point for cond. shut-off
        int CondCtrlType;                // Condensation control type (initialize to simple off)
        int CondErrIndex;                // Error index for recurring warning messages
        int NumCircCalcMethod;           // Calculation method for number of circuits per surface; 1=1 per surface, 2=use cicuit length
        Real64 CircLength;               // Circuit length {m}
        int GlycolIndex;                 // Index to fluid properties routines for water
        Real64 LengthRink;               // Length of ice rink
        Real64 WidthRink;                // Width of ice rink
        Real64 DepthRink;                // Depth of ice rink
        Real64 IceThickness;             // Thickness of ice surface
        Real64 SurfaceArea;              // Surface area of the rink
        Real64 FloodWaterTemp;           // Temperature of flood water used in the beginning of freezing the rink
        int CRefrigLoopNum;              // Cold refrigerant loop number
        int CRefrigLoopSide;
        int CRefrigBranchNum;
        int CRefrigCompNum;
        Real64 RefrigMassFlowRate;   // Refrigerant mass flow rate
        bool CondCausedShutDown;     // .TRUE. when condensation predicted at surface
        Real64 RefOutBOTCCtrlTemp;   // Outlet temperature of brine (To be )
        std::string PeopleSchedName; // surface name of rink floor
        int PeopleSchedPtr;          // index to schedule of people
        Real64 PeopleHeatGain;       // Heat gain from people (Watt/m2)
        Real64 SpectatorArea;        // Area over which spectators are present
        Real64 RefrigInletTemp;      // Temperature at which refrigerant enters

        // ReportData
        Real64 RefrigInletTemp;         // Refrigerat inlet temperature
        Real64 RefrigOutletTemp;        // Refrigerant outlet temperature
        Real64 RefrigMassFlowRate;      // Refrigerant mass flow rate
        Real64 CoolPower;               // Cooling sent to rink floor in Watts
        Real64 CoolEnergy;              // Cooling sent to rink floor in Joules
        // Default Constructor
        DirectRefrigSysData()
            : SchedPtr(0), ZonePtr(0), SurfacePtr(0), NumOfSurfaces(0), TubeDiameter(0.0), TubeLength(0.0), ControlType(0), RefrigVolFlowMaxCool(0.0),
              ColdRefrigInNode(0), ColdRefrigOutNode(0), ColdThrottleRange(0.0), ColdSetptSchedPtr(0), CondCtrlType(0), CondDewPtDeltaT(0.0),
              NumCircCalcMethod(0), CircLength(0.0), GlycolIndex(0), LengthRink(0.0), WidthRink(0.0), DepthRink(0.0), IceThickness(0.0),
              SurfaceArea(0.0),
              FloodWaterTemp(0.0), CRefrigLoopSide(0), CRefrigBranchNum(0), CRefrigCompNum(0), RefrigMassFlowRate(0.0), CondCausedShutDown(false),
              CondErrIndex(0), RefrigFlowMaxCool(0.0), RefrigFlowMinCool(0.0), RefOutBOTCCtrlTemp(0.0), PeopleSchedPtr(0), PeopleHeatGain(0.0), 
              SpectatorArea(0.0), RefrigInletTemp(0.0), RefrigOutletTemp(0.0), RefrigMassFlowRate(0.0), CoolPower(0.0),
              CoolEnergy(0.0)

        {
        }
    };

    struct IndirectRefrigSysData
    {
        // Members
        // Input Data
        std::string Name;            // name of indirect refrigeration system
        std::string RefrigerantName; // Name of refrigerant, must match name in FluidName
                                     //    (see fluidpropertiesrefdata.idf)
        int RefIndex;                // Index number of refrigerant, automatically assigned on first call to fluid property
                                     //   and used thereafter

        std::string SchedName;           // availability schedule
        int SchedPtr;                    // index to schedule
        std::string ZoneName;            // Name of zone the system is serving
        int ZonePtr;                     // Point to this zone in the Zone derived type
        std::string SurfaceName;         // surface name of rink floor
        int SurfacePtr;                  // index to a surface
        Array1D_int SurfacePtrArray;     // index to a surface array
        int NumOfSurfaces;               // Number of surfaces included in this refrigeration system (coordinated control)
        Array1D<Real64> SurfaceFlowFrac; // Fraction of flow/pipe length for the floor surface
        Array1D<Real64> NumCircuits;     // Number of fluid circuits in the surface
        Real64 TubeDiameter;             // tube diameter for embedded tubing
        Real64 TubeLength;               // tube length embedded in radiant surface
        int ControlType;                 // Control type for the system(BOTC or STC)
        Real64 RefrigVolFlowMaxCool;     // maximum refrigerant flow rate for cooling, m3/s
        Real64 RefrigFlowMaxCool;        // maximum refrigerant mass flow rate for cooling. Kg/s
        Real64 RefrigFlowMinCool;        // manimum refrigerant mass flow rate for cooling. Kg/s
        int ColdRefrigInNode;            // cold refrigerant inlet node
        int ColdRefrigOutNode;           // cold refrigerant Outlet node
        Real64 ColdThrottleRange;        // Throttling range for cooling [C]
        std::string ColdSetptSched;      // Schedule name for the ice rink setpoint temperature
        int ColdSetptSchedPtr;           // Schedule index for the ice rink setpoint temperature
        Real64 CondDewPtDeltaT;          // Diff between surface temperature and dew point for cond. shut-off
        int CondCtrlType;                // Condensation control type (initialize to simple off)
        int CondErrIndex;                // Error index for recurring warning messages
        int NumCircCalcMethod;           // Calculation method for number of circuits per surface; 1=1 per surface, 2=use cicuit length
        Real64 CircLength;               // Circuit length {m}
        int GlycolIndex;                 // Index to fluid properties routines for water
        Real64 LengthRink;               // Length of ice rink
        Real64 WidthRink;                // Width of ice rink
        Real64 DepthRink;                // Depth of ice rink
        Real64 IceThickness;             // Thickness of ice surface
        int CRefrigLoopNum;              // Cold refrigerant loop number
        int CRefrigLoopSide;
        int CRefrigBranchNum;
        int CRefrigCompNum;
        Real64 RefrigMassFlowRate; // Refrigerant mass flow rate
        bool CondCausedShutDown;   // .TRUE. when condensation predicted at surface
        Real64 RefOutBOTCtrlTemp;  // Outlet temperature of brine (To be )
        Real64 Concentration;      // Concentration of the brine used in the secondary refrigeration system
        int RefrigType;            // Type of secondary refrigerant, EG or CaCl2
        // ReportData

        // Default Constructor
        IndirectRefrigSysData()
            : SchedPtr(0), ZonePtr(0), SurfacePtr(0), NumOfSurfaces(0), TubeDiameter(0.0), TubeLength(0.0), ControlType(0), RefrigVolFlowMaxCool(0.0),
              ColdRefrigInNode(0), ColdRefrigOutNode(0), ColdThrottleRange(0.0), ColdSetptSchedPtr(0), CondCtrlType(0), CondDewPtDeltaT(0.0),
              NumCircCalcMethod(0), CircLength(0.0), GlycolIndex(0), LengthRink(0.0), WidthRink(0.0), DepthRink(0.0), IceThickness(0.0),
              CRefrigLoopSide(0), CRefrigBranchNum(0), CRefrigCompNum(0), RefrigMassFlowRate(0.0), CondCausedShutDown(false), CondErrIndex(0),
              RefrigFlowMaxCool(0.0), RefrigFlowMinCool(0.0), RefOutBOTCtrlTemp(0.0)

        {
        }
    };

    struct RefrigSysTypeData
    {
        // Members
        // This type used to track different components/types for efficiency
        std::string Name; // name of refrigeartion system
        int SystemType;   // Type of System (see System Types in Parameters)
        int CompIndex;    // Index in specific system types

        // Default Constructor
        RefrigSysTypeData() : SystemType(0), CompIndex(0)
        {
        }
    };

    struct ResurfacerData
    {
        // Members
        std::string Name;
        int GlycolIndex;
        int ResurfacingSchedPtr;
        Real64 ResurfacingWaterTemp;
        // Report Data
        Real64 QResurfacing;
        Real64 EHeatingWater;
        Real64 QHumidity;
        // Default Constructor
        ResurfacerData() : GlycolIndex(0), ResurfacingSchedPtr(0), ResurfacingWaterTemp(0.0), QResurfacing(0.0), EHeatingWater(0.0), QHumidity(0.0)
        {
        }
    };

    // Object Data:
    extern Array1D<DirectRefrigSysData> DRink;
    extern Array1D<IndirectRefrigSysData> IRink;
    extern Array1D<RefrigSysTypeData> RefrigSysTypes;
    extern Array1D<ResurfacerData> Resurfacer;

    // Functions:

    void GetIndoorIceRink();

    void InitIndoorIceRink(bool const FirstHVACIteration, 
                           int const SysNum,              
                           int const SystemType,
                           bool &InitErrorsFound);

    Real64 IceRinkResurfacer(Real64 ResurfacerTank_capacity,
                             Real64 ResurfacingHWTemperature,
                             Real64 IceSurfaceTemperature,
                             Real64 InitResurfWaterTemp,
                             int const ResurfacerIndex,
                             int const SysNum);

    Real64 CalcDRinkHXEffectTerm(Real64 const Temperature,    // Temperature of refrigerant entering the radiant system, in C
                                 int const SysNum,            // Index to the refrigeration system
                                 Real64 const RefrigMassFlow, // Mass flow rate of refrigerant in direct refrigeration system, kg/s
                                 Real64 TubeLength,           // Total length of the piping used in the radiant system
                                 Real64 TubeDiameter);

    Real64 CalcIRinkHXEffectTerm(Real64 const Temperature,    // Temperature of the refrigerant entering the radiant system
                                 int const SysNum,            // Index to the refrigeration system
                                 Real64 const RefrigMassFlow, // Mass flow rate of refrigerant in direct refrigeration system, kg/s
                                 Real64 TubeLength,           // Total length of the piping used in the radiant system
                                 Real64 TubeDiameter,         // Inner diameter of the piping used in the radiant system
                                 int const RefrigType, // Refrigerant used in the radiant system: Ethylene Glycol(EG) or Cslcium Chloride(CaCl2)
                                 Real64 Concentration  // Concentration of the brine(refrigerant) in the radiant system (allowed range 10% to 30%)
    );

    void CalcDirectIndoorIceRinkComps(int const SysNum, // Index number for the indirect refrigeration system
                                      Real64 &LoadMet);

    void CalcDirectIndoorIceRinkSys(int const SysNum, // name of the direct refrigeration system
                                    Real64 &LoadMet   // load met by the direct refrigeration system, in Watts
    );

    Real64 SumHATsurf(int const ZoneNum);

    Real64 BOTC(int const SystemType, int const SysNum, Real64 const Temperature);

    Real64 STC(int const SystemType, int const SysNum);

} // namespace IceRink
} // namespace EnergyPlus

#endif
