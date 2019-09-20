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

    extern Array1D_bool CheckEquipName;

    struct DirectRefrigSysData
    {
        // Members
        // Input data
        std::string Name;                // name of direct refrigeration system
        std::string SchedName;           // availability schedule
        int SchedPtr;                    // index to schedule
        std::string ZoneName;            // Name of zone the system is serving
        int ZonePtr;                     // Point to this zone in the Zone derived type
        std::string SurfaceName;         // surface name of rink floor
        int SurfacePtr;                  // index to surface array
        int NumOfSurfaces;               // Number of surfaces included in this refrigeration system (coordinated control)
        Array1D<Real64> SurfaceFlowFrac; // Fraction of flow/pipe length for the floor surface
        Array1D<Real64> NumCircuits;     // Number of fluid circuits in the surface
        Real64 TubeDiameter;             // tube diameter for embedded tubing
        Real64 TubeLength;               // tube length embedded in radiant surface
        int ControlType;                 // Control type for the system(BOTC or STC)
        Real64 RefrigVolFlowMaxCool;     // maximum refrigerant flow rate for cooling, m3/s
        int ColdRefrigInNode;            // cold refrigerant inlet node
        int ColdRefrigOutNode;           // cold refrigerant Outlet node
        Real64 ColdThrottleRange;        // Throttling range for cooling [C]
        std::string ColdSetptSched;      // Schedule name for the ice rink setpoint temperature
        int ColdSetptSchedPtr;           // Schedule index for the ice rink setpoint temperature
        Real64 CondDewPtDeltaT;          // Diff between surface temperature and dew point for cond. shut-off
        int CondCtrlType;                // Condensation control type (initialize to simple off)
        int NumCircCalcMethod;           // Calculation method for number of circuits per surface; 1=1 per surface, 2=use cicuit length
        Real64 CircLength;               // Circuit length {m}
        int GlycolIndex;                 // Index to Glycol (Ammonia) Properties
        Real64 LengthRink;               // Length of ice rink
        Real64 WidthRink;                // Width of ice rink
        Real64 DepthRink;                // Depth of ice rink
        int CRefrigLoopNum;              // Cold refrigerant loop number

        // ReportData

        // Default Constructor
        DirectRefrigSysData()
            : SchedPtr(0), ZonePtr(0), SurfacePtr(0), NumOfSurfaces(0), TubeDiameter(0.0), TubeLength(0.0), ControlType(0), RefrigVolFlowMaxCool(0.0),
              ColdRefrigInNode(0), ColdRefrigOutNode(0), ColdThrottleRange(0.0), ColdSetptSchedPtr(0), CondCtrlType(0), CondDewPtDeltaT(0.0),
              NumCircCalcMethod(0), CircLength(0.0), GlycolIndex(0), LengthRink(0.0), WidthRink(0.0), DepthRink(0.0)

        {
        }
    };

    struct IndirectRefrigSysData
    {
        // Members
        // Input Data
        std::string Name; // name of indirect refrigeration system
        int SchedPtr;     // index to schedule
        int GlycolIndex;  // Index to Glycol (Brine) Properties

        Real64 TubeDiameter; // tube diameter for embedded tubing
        Real64 TubeLength;   // tube length embedded in radiant surface

        // Report Data

        // Default Constructor
        IndirectRefrigSysData() : SchedPtr(0), GlycolIndex(0)
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

   

} // namespace IceRink
} // namespace EnergyPlus

#endif
