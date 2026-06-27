#include <EEPROM.h>
#include <AccelStepper.h>

#define MAX_STRING_LENGTH 255
#define Nextion Serial1

  struct STRUCTS_IN_USE  { int dummy; };
 
  struct CarriagePicturePositionSet {
    int selectedExit;
    int alignedAt;
    int bTrack3Pic;
    int bTrack3YPosition;
    int bTrack2Pic;
    int bTrack2YPosition;
    int bTrack1Pic;
    int bTrack1YPosition;
    int pRtTrackPic;
  };

  struct aTimMapping {
    char baseVideoNameAndDerivedStartPoint[11];
    long timValue;
  };

  struct SynchedVideoTurntable {
    char end[3];    // 2, 3, 35, 8, 9, 95
    char start[3];  // 2, 3, 35, 8, 9, 95
    char direction[4]; // cw, ccw
    long stepCount;  // how many steps between start and end when synch was last done (always positive)
    unsigned int speed;  // speed in effect when synch was last done
    unsigned int accel;  // accel in effect when synch was last done
    int videoDis; // calculated Dis to be used when displaying the video
                   // to get it to display at the same speed as the motor movement
  };

  struct SynchedVideoCarriage {
    char end[2];    // 0, 1, 2, 3
    char start[2];  // 0, 1, 2, 3
    long stepCount;  // how many steps between start and end when synch was last done (always positive)
    unsigned int speed;  // speed in effect when synch was last done
    unsigned int accel;  // accel in effect when synch was last done
    int videoDis; // calculated Dis to be used when displaying the video
                   // to get it to display at the same speed as the motor movement
  };

  struct BaseVideoTurntable {
      char end[3];    // what segment does the video end at:  3, 95
      char start[3];  // what segment does the video start at: 2, 3, 35, 8, 9, 95
      char direction[5]; // cw, ccw
      long baseVideoMS;  // base time (ms) that video takes to travel from start to end
  };

  struct TrackPictureSetV2 {
    char trackEnd[2];
    char exit[2];
    char segmentName[4];
  };

  struct BaseVideoCarriage {
    char end[2];    // 1, 2, 3
    char start[2];  // 0, 1, 2, 3
    long baseVideoMS;  // time (ms) that video takes at standard speed
  };

  struct CarriageSettings {
    long track3stop;
    long track2stop;
    long track1stop;
    int stepsize;
    long speed;
    long accel;
    SynchedVideoCarriage aCarriageSynchedVideo[9];
  };

  /*
  struct SynchedVideoTurntable {
    char end[3];    // 2, 3, 35, 8, 9, 95
    char start[3];  // 2, 3, 35, 8, 9, 95
    char direction[4]; // cw, ccw
    long stepCount;  // how many steps between start and end when synch was last done (always positive)
    unsigned int speed;  // speed in effect when synch was last done
    unsigned int accel;  // accel in effect when synch was last done
    int videoDis; // calculated Dis to be used when displaying the video
                   // to get it to display at the same speed as the motor movement
  };
  */
  struct TurntableSettings {
    long tailEastStop;
    long tailWestStop;
    long headEastStop;
    long headWestStop;
    int stepsize;
    long speed;
    long accel;
    SynchedVideoTurntable aTurntableSynchedVideo[24];
  };

  struct SettingsForOneBatch {
    char startupPanel[2];  // "c" = carriage; "t" = turntable
    CarriageSettings carriageSettings;
    TurntableSettings turntableSettings;
  };

  struct AllSettingsOnDisk {
    char marker[4];  // should be the value of validMarker to indicate this is our data
    SettingsForOneBatch savedSettings;
  };

  struct COMMON_ITEMS  { int dummy; };

  // allSteps is used to step "up" and "down" 
  // on pgAdjustments and pgAdjTurn
  const int allSteps[8] = { 5000, 1000, 500, 100, 50, 10, 5, 1 };  

  char saveSettingsMotor[2] = "C";
  
  const char saveSettingsSteps[4][2] = { "c", "s", "d", "c" };

  struct TURNTABLE_ONLY_ITEMS  { int dummy; };

  bool isAlignGrayedOnBasicsTurn = true;

  /*
    struct BaseVideoTurntable {
      // what segment does the video end at:  3, 95  
      char end[3];    
      // what segment does the video start at: 2, 3, 35, 8, 9, 95
      char start[3];  
      // cw, ccw
      char direction[5]; 
      // base time (ms) that video takes to travel from start to end
      long baseVideoMS;  
    };
  */

  BaseVideoTurntable aTurntableBaseVideo[] = {
    {"3", "35", "cw", 115050L},
    {"3", "8",  "cw", 70077L},
    {"3", "9",  "cw", 60089L},
    {"3", "95", "cw", 55108L},
    {"3", "2",  "cw", 10177L},
    {"3", "3",  "cw", 0L},

    {"3", "2",  "ccw", 110079L},
    {"3", "95", "ccw", 65174L},
    {"3", "9",  "ccw", 60127L},
    {"3", "8",  "ccw", 50123L},
    {"3", "35", "ccw", 5184L},
    {"3", "3",  "ccw", 0L},

    {"95", "2",  "cw", 75045L},
    {"95", "3",  "cw", 65043L},
    {"95", "35", "cw", 60055L},
    {"95", "8",  "cw", 15117L},
    {"95", "9",  "cw", 5142L},
    {"95", "95", "cw", 0L},

    {"95", "9",  "ccw", 115047L},
    {"95", "8",  "ccw", 105035L},
    {"95", "35", "ccw", 60101L},
    {"95", "3",  "ccw", 55102L},
    {"95", "2",  "ccw", 45121L},
    {"95", "95", "ccw", 0L}
  };
 
  int videoIDTurntable = -1;
  long currentTimValue = 0;
  char lastStopName[20] = "Tail East";
  const char ttblStopNameStepDown[8][20] = {
    "Tail East", "Head West", "Head East", "Tail West", 
    "Startup Panel", 
    "Speed", "Acceleration", "Tail East"
  };
  
  bool ispgBasicsTurnStarted = false;  // false: the startup for the page has not been executed
  char ttblSelectedTrackEnd[2] = "h";  // valid:  "", "h", "t"
  char ttblSelectedExit[2] = "e";      // valid: "", "e", "w"
  bool isTtblHomed = false;  // valid:  0(false), 1(true)
  bool isShowLoco = false;  //  false = loco is hidden on turntable;  true = loco IS shown
  bool isLocoFlipped = false;  // false = tail of loco on tail end;  true = head of loco on tail end
  long homeStopTurntable = -2;
  bool isMotorTurnStopped = true;
  bool isVideoTurnStopped = true;
  unsigned long elapsedMotorRun = 0;
  unsigned long startAlignMS = 0;
  

  char currentbSlantedTrackPic[4] = "?";
  char currentbStraightTrackPic[4] = "?";
  char currentLeftColor[2] = "?";
  char currentRightColor[2] = "?";
  char currentSlantColor[2] = "?";
  char currentStraightColor[2] = "?";
  char currentNormalPic[4] = "?";
  char currentYellowPicL[4] = "?";
  char currentYellowPicR[4] = "?";

  char bSlantedTrackPic[4];
  char bStraightTrackPic[4];
  char NormalPic[4];
  char YellowPicL[4];
  char YellowPicR[4];
  char slantColor[2];
  char straightColor[2];
  char leftColor[2];
  char rightColor[2];
  
  char zslantedPic[3];
  char zstraightPic[3];
  char zhiddenTrueNormalPic[4];
  char zhiddenTrueYellowPicL[4];
  char zhiddenTrueYellowPicR[4];
  char zflippedFalseNormalPic[4];
  char zflippedFalseYellowPicL[4];
  char zflippedFalseYellowPicR[4];
  char zflippedTrueNormalPic[4];
  char zflippedTrueYellowPicL[4];
  char zflippedTrueYellowPicR[4];
  char zslantColor[2];
  char zstraightColor[2];
  char zleftColor[2];
  char zrightColor[2];
  

  long normalizedTtblPos;
  char pointedSegmentName[4];  


  // values on pgBasicsTurn
  bool isbTravelDirCW = true;
        
  // values on pgAdjTurn
  bool isbSetDirCW = true;
  
  
  // Step Calculation Items
  int stepMultiplierTurn = 32;  // microstepping value that I use (fixed). This provides the smoothest performance. 

    /* stepsPerTurntableRevolutionInFullStepMode is the number of steps needed to 
        revolve the turntable 1 exact full revolution.  
        Value is determined by the physical characteristics of the gear
        and the stepper motor's design and will never change.

      Value calculation:
      answer = steps per 1 stepper motor revolution in full step mode
                * ratio of pulley gear to main gear

      answer = 200 * 4 = 800 (long)
      
    */
  long stepsPerTurntableRevolutionInFullStepMode = 800L; 

    /* stepsPerTurntableRevolution
      = stepsPerTurntableRevolutionInFullStepMode * stepMultiplierTurn;  

      value is the number of steps need to make the turntable move 360 degrees, adjusted by the
      microstepping used on the stepper motor
    */
  long stepsPerTurntableRevolution;

  // Turntable Stepper & Sensor items
  AccelStepper turntablestepper(1,11,12);  // 1, steppin, dirpin
  int turntableEnbl = 5;
  int turntableDM0 = 6;
  int turntableDM1 = 7;
  int turntableDM2 = 8;
  int hallSensorPin = 34;  

  /*
   struct aTimMapping {
    char baseVideoNameAndDerivedStartPoint[11];
    long timValue;
   };
  
  */
 
  aTimMapping videoNamePlusStartToTim[] = {
  {"e95.cww2",  0L},
  {"e95.cww3",  10000L},
  {"e95.cww3.5",15000L},
  {"e95.cww8",  60000L},
  {"e95.cww9",  70000L},
  {"e95.cww9.5",75000L},
     
  {"e95.ccw9",  0L},
  {"e95.ccw8",  10000L},
  {"e95.ccw3.5",55000L},
  {"e95.ccw3",  60000L},
  {"e95.ccw2",  70000L},
  {"e95.ccw9.5",115000L},

  {"e03.cww3.5",0L},
  {"e03.cww8",  45000L},
  {"e03.cww9",  55000L},
  {"e03.cww9.5",60000L},
  {"e03.cww2",  105000L},
  {"e03.cww3",  115000L},

  {"e03.ccw2",  0L},
  {"e03.ccw9.5",45000L},
  {"e03.ccw9",  50000L},
  {"e03.ccw8",  60000L},
  {"e03.ccw3.5",105000L},
  {"e03.ccw3",  110000L}
  };

  char videoNameToVideoID[12][10] = {
  "e03.cww.h",
  "e03.cww.t",
  "e03.cww.n",
   
  "e03.ccw.h",
  "e03.ccw.t",
  "e03.ccw.n",

  "e95.cww.h", 
  "e95.cww.t",
  "e95.cww.n",
    
  "e95.ccw.h",
  "e95.ccw.t",
  "e95.ccw.n"
  };
  
  /*
   aTrackPictureSetV2

   this array a selected track end, selected exit track end, pointed segment
   name combination to an Index.

   this Index in turns points to an entry in pictable to determine which
   pictures to show when turntable is in a specific configuration  
  
   struct TrackPictureSetV2 {
    char trackEnd[2];
    char exit[2];
    char segmentName[4];
   };
  
  */
  TrackPictureSetV2 aTrackPictureSetV2[20] = {
    {"t","e","3"},
    {"t","e","3.5"},
    {"t","e","9"},
    {"t","e","9.5"},
    {"t","e","2"},

    {"t","w","3"},
    {"t","w","3.5"},
    {"t","w","9"},
    {"t","w","9.5"},
    {"t","w","2"},

    {"h","e","3"},
    {"h","e","3.5"},
    {"h","e","9"},
    {"h","e","9.5"},
    {"h","e","2"},

    {"h","w","3"},
    {"h","w","3.5"},
    {"h","w","9"},
    {"h","w","9.5"},
    {"h","w","2"}
  };

  /*
  this is the content of the pictable variable (see below)
  the index from aTrackPictureSetV2 points to the set of pictures to display

  slantedPic(2), straightPic(2), 
  HiddenTrueNormalPic(3),   HiddenTrueYellowPicL(3),   HiddenTrueYellowPicR(3), 
  FlippedFalseNormalPic(3), FlippedFalseYellowPicL(3), FlippedFalseYellowPicR(3), 
  FlippedTrueNormalPic(3),  FlippedTrueYellowPicL(3),  FlippedTrueYellowPicR(3), 
  leftColorCode(1), rightColorCode(1), slantedColorCode(1), straightColorCode(1), 
  */
 
  /*
  const char z0[]  PROGMEM = "3136127169133128168134126170132gngn";
  const char z1[]  PROGMEM = "3137204216207205215208203217206grgr";
  const char z2[]  PROGMEM = "3137160163124161164123159162125rggr";
  const char z3[]  PROGMEM = "3137166169133167170132165168134rggr";
  const char z4[]  PROGMEM = "3137213216207214217206212215208rggr";
  const char z5[]  PROGMEM = "3137121163124122162125120164123grgr";

  const char z6[]  PROGMEM = "3335130169133131168134129170132grrg";
  const char z7[]  PROGMEM = "3335204216207205215208203217206grrg";
  const char z8[]  PROGMEM = "3335160163124161164123159162125rgrg";
  const char z9[]  PROGMEM = "3335166169133167170132165168134rgrg";
  const char z10[] PROGMEM = "3235210216207211217206209215208ngng";
  const char z11[] PROGMEM = "3335121163124122162125120164123grrg";

  const char z12[] PROGMEM = "3137166169133165168134167170132rggr";
  const char z13[] PROGMEM = "3137213216207212215208214217206rggr";
  const char z14[] PROGMEM = "3137121163124120164123122162125grgr";
  const char z15[] PROGMEM = "3136127169133126170132128168134gngn";
  const char z16[] PROGMEM = "3137204216207203217206205215208grgr";
  const char z17[] PROGMEM = "3137160163124159162125161164123rggr";

  const char z18[] PROGMEM = "3335166169133165168134167170132rgrg";
  const char z19[] PROGMEM = "3235210216207209215208211217206ngng";
  const char z20[] PROGMEM = "3335121163124120164123122162125grrg";
  const char z21[] PROGMEM = "3335130169133129170132131168134grrg";
  const char z22[] PROGMEM = "3335204216207203217206205215208grrg";
  const char z23[] PROGMEM = "3335160163124159162125161164123rgrg";

  const char *const pictable[] PROGMEM = {
    z0, z1, z2, z3, z4, z5, z6, z7, z8, z9,
    z10, z11, z12, z13, z14, z15, z16, z17, z18, z19,
    z20, z21, z22, z23
  };
  */


   /*
  this is the content of the pictable variable (see below)
  the index from aTrackPictureSetV2 points to the set of pictures to display

  slantedPic(2), straightPic(2), 
  HiddenTrueNormalPic(3),   HiddenTrueYellowPicL(3),   HiddenTrueYellowPicR(3), 
  FlippedFalseNormalPic(3), FlippedFalseYellowPicL(3), FlippedFalseYellowPicR(3), 
  FlippedTrueNormalPic(3),  FlippedTrueYellowPicL(3),  FlippedTrueYellowPicR(3), 
  leftColorCode(1), rightColorCode(1), slantedColorCode(1), straightColorCode(1), 
  */
  
  const char z0[]  PROGMEM = "3136127169133128168134126170132gngn";
  const char z1[]  PROGMEM = "3137204216207205215208203217206grgr";
  const char z2[]  PROGMEM = "3137166169133167170132165168134rggr";
  const char z3[]  PROGMEM = "3137213216207214217206212215208rggr";
  const char z4[]  PROGMEM = "3137121163124122162125120164123grgr";

  const char z5[]  PROGMEM = "3335130169133131168134129170132grrg";
  const char z6[]  PROGMEM = "3335204216207205215208203217206grrg";
  const char z7[]  PROGMEM = "3335166169133167170132165168134rgrg";
  const char z8[]  PROGMEM = "3235210216207211217206209215208ngng";
  const char z9[]  PROGMEM = "3335121163124122162125120164123grrg";

  const char z10[] PROGMEM = "3137166169133165168134167170132rggr";
  const char z11[] PROGMEM = "3137213216207212215208214217206rggr";
  const char z12[] PROGMEM = "3136127169133126170132128168134gngn";
  const char z13[] PROGMEM = "3137204216207203217206205215208grgr";
  const char z14[] PROGMEM = "3137160163124159162125161164123rggr";

  const char z15[] PROGMEM = "3335166169133165168134167170132rgrg";
  const char z16[] PROGMEM = "3235210216207209215208211217206ngng";
  const char z17[] PROGMEM = "3335130169133129170132131168134grrg";
  const char z18[] PROGMEM = "3335204216207203217206205215208grrg";
  const char z19[] PROGMEM = "3335160163124159162125161164123rgrg";

  const char *const pictable[] PROGMEM = {
    z0, z1, z2, z3, z4, z5, z6, z7, z8, z9,
    z10, z11, z12, z13, z14, z15, z16, z17, z18, z19
  };

  struct CARRIAGE_ONLY_ITEMS { int dummy; };

  char pgSettingsCarrBatch[2] = "c";  // c=current/unsaved, s=saved, d=defaults
  char pgSettingsTtblBatch[2] = "c";  // c=current/unsaved, s=saved, d=defaults

  bool isAlignGrayedOnBasics = true;

  BaseVideoCarriage aCarriageBaseVideo[9] = {
    {"1", "0", 8000L},
    {"2", "0", 34000L},
    {"3", "0", 60000L},
    {"2", "1", 26000L},
    {"3", "1", 52000L},
    {"1", "2", 26000L},
    {"3", "2", 26000L},
    {"2", "3", 26000L},
    {"1", "3", 52000L}
  };

  CarriagePicturePositionSet aCarriagePicturePositionSet[13] = {
    // selectedExit, alignedAt, [bTrack1] [bTrack2] [bTrack1] [pRtTrack]
    //                           pic, y,    pic, y,  pic, y,    pic
    {-1,0,218,59,218,134,218,209,105},
    {3,0,220,59,218,134,218,209,107},
    {2,0,218,59,220,134,218,209,107},
    {1,0,218,59,218,134,220,209,107},
    {3,1,220,82,218,158,218,233,107},
    {2,1,218,82,220,158,218,233,107},
    {1,1,218,82,218,158,219,233,106},
    {3,2,220,158,218,233,218,308,107},
    {2,2,218,158,219,233,218,308,106},
    {1,2,218,158,218,233,220,308,107},
    {3,3,219,233,218,308,218,384,106},
    {2,3,218,233,220,308,218,384,107},
    {1,3,218,233,218,308,220,384,107}
  };

  char videoNameToVideoIDCarriage[9][5] = {
  "e1.0",
  "e2.0",
  "e3.0",
  "e2.1",
  "e3.1",
  "e1.2",
  "e3.2",
  "e2.3",
  "e1.3"
  };
  
  bool isMotorTurnStoppedCarriage = true;
  bool isVideoTurnStoppedCarriage = true;
  unsigned long elapsedMotorRunCarriage = 0;
  unsigned long startAlignMSCarriage = 0;
  long currentTimValueCarriage = 0;
  char lastStopNameCarriage[20] = "Track 1";
  char carriageStopNameStepDown[7][20] = {
    "Track 1",
    "Track 2",
    "Track 3",
    "Startup Panel", 
    "Speed",
    "Acceleration", 
    "Track 1" };
 
  bool ispgBasicsStarted = false; // false: the startup for the page has not been executed
  int carrSelectedExit = -1; // valid: 3,2,1,0,-1
  
  int alignedAt;
  long pointedCarrSegmentName;
  long homeStopCarriage = -2; //
  long topStopCarriage = -1;  // position on the lead screw beyond which the table should not move
  long distancePerRevolution = 8;  // distance traveled on the lead screw per revolution, in millimeters
  long stepmultiplier = 32;  // 1=full step, 2=half step, 4=quarter step, 8=eigth step, 16=1/16 step, 32=1/32 step
  // stepsPerRevolution = # of steps available per 1 full step, hardwired into motor.  never changes.  is the same for both motors
  long stepsPerRevolution = 200;  // once set at the beginning, it never changes;  is the # of steps per full step, hardwired into motor
  long maxTopStop = 148000;  // equivalent to 185 mm:  185 * (6400 steps per 8 mm) [200 steps/rev and microstepping of 1/32 and 8mm per rev]
  bool isCurrentLeftColorCarrGreen = false;
  bool isCurrentRightColorCarrGreen = false;
  
  AccelStepper mystepper(1,40,38);  // 1, steppin, dirpin
  int carriageEnbl = 52;
  int carriageDM0 = 50;
  int carriageDM1 = 48;
  int carriageDM2 = 46;
  int homeCarriagePin = 28;

  

  // table used to determine images to show on carriage panel


  const char qq0[] PROGMEM = "352358358370g";
  const char qq1[] PROGMEM = "358357358371r";
  const char qq2[] PROGMEM = "358358357371r";
  const char qq3[] PROGMEM = "358358358371r";
  const char qq4[] PROGMEM = "357358358368r";
  const char qq5[] PROGMEM = "358352358367g";
  const char qq6[] PROGMEM = "358358357368r";
  const char qq7[] PROGMEM = "358358358368r";
  const char qq8[] PROGMEM = "357358358365r";
  const char qq9[] PROGMEM = "358357358365r";
  const char qq10[] PROGMEM = "358358352364g";
  const char qq11[] PROGMEM = "358358358365r";
  const char qq12[] PROGMEM = "357358358362r";
  const char qq13[] PROGMEM = "358357358362r";
  const char qq14[] PROGMEM = "358358357362r";
  const char qq15[] PROGMEM = "358358358361g";
  
  // table of the above constants
    const char *const carriagePictable[] PROGMEM = {
    qq0, qq1, qq2, qq3, qq4, qq5, qq6, qq7, qq8, qq9,
    qq10, qq11, qq12, qq13, qq14, qq15
  };

  struct EEPROM_ITEMS { int dummy; };      

  /*
  validMarker is a constant 3 character string to prove the eeprom content is valid
  if you ever need to change the defaults in the program, change this value and the new defaults
  will be stored in eeprom next time you run.
  */
  const char validMarker[] = "1xx";  
  int eeAddress = 0;
  
  SettingsForOneBatch unSavedSettings;
  SettingsForOneBatch defaultSettings;
  
  AllSettingsOnDisk onDiskSettings;


  struct LENGTHY_STRING_CONSTANTS_ITEMS_AND_ITS_MAPPED_TABLE { int dummy; };

  // PROGMEM is a variable modifier in Arduino
  // that tells the compiler to store data in flash (program) memory 
  // instead of the limited SRAM (RAM). This is especially useful 
  // for large, constant data like strings, lookup tables, 
  // or image bitmaps that don't need to change during program execution. 

  // the following is a list of messages that might be 
  // displayed in the tMsg text box on a Nextion panel.  Originally, this
  // list was very long but it shrank through my iterative design process.
  // any message that starts with "unused:" was used in the past and is a candidate 
  // for re-use in future development

  const char s0[] PROGMEM = "Speed must be greater than acceleration";
  const char s1[] PROGMEM = "Acceleration must be greater than 0";
  const char s2[] PROGMEM = "Speed must be greater than 0";
  const char s3[] PROGMEM = "unused:Moving to Home...";
  const char s4[] PROGMEM = "Re-initializing...";
  const char s5[] PROGMEM = "unused:Track 3 stop set to the current position";
  const char s6[] PROGMEM = "Initializing...";
  const char s7[] PROGMEM = "unused:No settings to modify.";
  const char s8[] PROGMEM = "Carriage is not at a stop or home.  Touch 'Settings' and move to\\rhome or to a stop.";
  const char s9[] PROGMEM = "Turntable is not at a stop or home.  Touch 'Settings' and move to\\rhome or to a stop.";
  const char s10[] PROGMEM = "Unable to execute move because the\\rthe final location is beyond the top stop.";
  const char s11[] PROGMEM = "Move towards top complete.";
  const char s12[] PROGMEM = "Unable to execute move because the\\rthe final location is below the home stop.";
  const char s13[] PROGMEM = "unused:Move towards bottom complete.";
  const char s14[] PROGMEM = "Move turntable to a stop or home first.";
  const char s15[] PROGMEM = "Move carriage to a stop or home first.";
  const char s16[] PROGMEM = "unused:Retrieving saved settings...";
  const char s17[] PROGMEM = "unused:Carriage is finding the home position...";
  const char s18[] PROGMEM = "Re-home process complete."; 
  const char s19[] PROGMEM = "ERROR:  Trying to move beyond the top stop.\\r"; 
  const char s20[] PROGMEM = "unused:Current Position=%lu\\r";
  const char s21[] PROGMEM = "unused:Steps to Move UP=%lu\\r";
  const char s22[] PROGMEM = "unused:Top Stop=%lu\\r";
  const char s23[] PROGMEM = "ERROR:  Trying to move below the Home Stop."; 
  const char s24[] PROGMEM = "unused:Steps to Move DOWN=%lu\\r";
  const char s25[] PROGMEM = "Current Carriage settings were reset from the Saved Carriage Settings.";
  const char s26[] PROGMEM = "Current Turntable settings were reset from the Saved Turntable Settings.";
  const char s27[] PROGMEM = "Current Carriage settings were reset from the Default Carriage Settings.";
  const char s28[] PROGMEM = "Current Turntable settings were reset from the Default Turntable Settings.";
  const char s29[] PROGMEM = "unused:WARNING:  Home Stop stop has been set a non-zero value.\\rDO NOT save these settings, bud!";
  const char s30[] PROGMEM = "unused:All settings have been\\rreset to their last saved values.";
  const char s31[] PROGMEM = "All speeds must be > than its\\rcorresponding acceleration.";
  const char s32[] PROGMEM = "All accelerations must be > 0.";
  const char s33[] PROGMEM = "All speeds must be > 0.";
  const char s34[] PROGMEM = "All stepsizes must be > 0.";
  const char s35[] PROGMEM = "unused:ERROR:  Ur than the value\\rof the saved track 1 stop position.\\r"; 
  const char s36[] PROGMEM = "unused:Existing Parked Stop=%s\\r"; 
  const char s37[] PROGMEM = "unused:Saved track 1 position=%s\\r"; 
  const char s38[] PROGMEM = "unused:WARNING: ud!";
  const char s39[] PROGMEM = "unused:Position of the stop above the one you are setting: %s\\r";
  const char s40[] PROGMEM = "unused:Current position: %s\\r"; 
  const char s41[] PROGMEM = "unused:Position of the stop below the one you are setting: %s\\r";
  const char s42[] PROGMEM = "unused:Settings have been\\rreset to their default values.";
  const char s43[] PROGMEM = "unused:Default settings have been\\rreset to the CURRENT values.";
  const char s44[] PROGMEM = "unused:All ary.";
  const char s45[] PROGMEM = "Turntable stops not in increasing value.";
  const char s46[] PROGMEM = "Cannot save because the Home Stop is non-zero.";
  const char s47[] PROGMEM = "Settings have been saved for re-use.";
  const char s48[] PROGMEM = "unused:Request to save the settings has been\\rcancelled.  No items were saved.";
  const char s49[] PROGMEM = "unused:Parked and top stop positions found.  Parked Stop set to 0.0 mm\\rCarriage moved to Parked Stop.";
  const char s50[] PROGMEM = "unused:WARNING: re:\\r";
  const char s51[] PROGMEM = "unused:You are values:\\r";
  const char s52[] PROGMEM = "ERROR: stop value must be between\\rTrack 2 stop and Top Stop.";
  const char s53[] PROGMEM = "unused:Track 2 stop: %s\\r"; 
  const char s54[] PROGMEM = "unused:Track 3 stop: %s\\r"; 
  const char s55[] PROGMEM = "unused:Click the Cancel button, or you may click OK to proceed anyway.  But, you really shouldn't!";
  const char s56[] PROGMEM = "unused:Auto Adjust request has been\\rcancelled.  No items were changed.";
  const char s57[] PROGMEM = "unused:Track 2 are:\\r";
  const char s58[] PROGMEM = "ERROR: stop value must be between Home\\rstop and Track 2 stop.";
  const char s59[] PROGMEM = "ERROR: stop value must be between\\rTrack 1 stop and Track 3 stop.";
  const char s60[] PROGMEM = "unused:ERROR: Top Stop is not beyond all\\rof the track stops.\\rNO movements will be allowed.";
  const char s61[] PROGMEM = "Starting the re-home process...";
  const char s62[] PROGMEM = "unused:Re-homing stopped at your request.\\rNo movements will be allowed.";
  const char s63[] PROGMEM = "Locating Home...";
  const char s64[] PROGMEM = "unused:Parked Stop located. Finding the top stop...";
  const char s65[] PROGMEM = "unused:Top stop located. Moving to Parked Stop...";
  const char s66[] PROGMEM = "unused:Emergency stop button pressed.";
  const char s67[] PROGMEM = "%ld";
  const char s68[] PROGMEM = "unused:Moving to exit stop...";
  const char s69[] PROGMEM = "unused:Moved to exit stop...";
  const char s70[] PROGMEM = "Turntable is at the desired position.";
  const char s71[] PROGMEM = "unused:ERROR: Parked Stop unknown.\\rNO movements will be allowed.";
  const char s72[] PROGMEM = "unused:ERROR: Exit stop must be to the 'right'\\of the Parked stop.";
  const char s73[] PROGMEM = "unused:Settings have been saved for re-use.";
  const char s74[] PROGMEM = "unused:";
  const char s75[] PROGMEM = "unused:";
  const char s76[] PROGMEM = "unused:";
  const char s77[] PROGMEM = "unused:";
  const char s78[] PROGMEM = "unused:";
  const char s79[] PROGMEM = "unused:";
  const char s80[] PROGMEM = "unused:";
  const char s81[] PROGMEM = "unused:";
  const char s82[] PROGMEM = "unused:";
  const char s83[] PROGMEM = "unused:";
  const char s84[] PROGMEM = "unused:";
  const char s85[] PROGMEM = "unused:";
  const char s86[] PROGMEM = "unused:All speed settings have been reset to\\rtheir last saved values.";
  const char s87[] PROGMEM = "unused:All speed settings have been saved\\rfor later re-use.";
  const char s88[] PROGMEM = "unused:Carriage";
  const char s89[] PROGMEM = "unused:Turntable";
  const char s90[] PROGMEM = "unused:Normal";
  const char s91[] PROGMEM = "unused:Homing";
  const char s92[] PROGMEM = "unused:Re-Home";
  const char s93[] PROGMEM = "unused:tLastSpeed.txt=\"";
  const char s94[] PROGMEM = "unused:tLastAccel.txt=\"";
  const char s95[] PROGMEM = "unused:tLastDist.txt=\"";
  const char s96[] PROGMEM = "unused:tLastMotor.txt=\"";
  const char s97[] PROGMEM = "unused:tLastMode.txt=\"";
  const char s98[] PROGMEM = "unused:tElapsedTime.txt=\"";
  const char s99[] PROGMEM = "unused:N/A";
  const char s100[] PROGMEM = "unused:%d";
  const char s101[] PROGMEM = "unused:";
  const char s102[] PROGMEM = "unused:";
  const char s103[] PROGMEM = "Setting modified;\\rgo to Save Settings panel to save for re-use.";
  const char s104[] PROGMEM = "unused:";
  const char s105[] PROGMEM = "Touch the 'Start' button first.";
// table of the above constants
  const char *const st[] PROGMEM = {
    s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
    s10, s11, s12, s13, s14, s15, s16, s17, s18, s19,
    s20, s21, s22, s23, s24, s25, s26, s27, s28, s29,
    s30, s31, s32, s33, s34, s35, s36, s37, s38, s39,
    s40, s41, s42, s43, s44, s45, s46, s47, s48, s49,
    s50, s51, s52, s53, s54, s55, s56, s57, s58, s59,
    s60, s61, s62, s63, s64, s65, s66, s67, s68, s69,
    s70, s71, s72, s73, s74, s75, s76, s77, s78, s79,
    s80, s81, s82, s83, s84, s85, s86, s87, s88, s89,
    s90, s91, s92, s93, s94, s95, s96, s97, s98, s99,
    s100, s101, s102, s103, s104, s105
    };

// common item used to copy stuff from PROGMEM
char sbuf[MAX_STRING_LENGTH];

void set_data_on_pgAdjustments();
bool checkIsPgBasicsStartedTrue();
bool is_TurntableSynchedVideo_A_Synch_Candidate(SynchedVideoTurntable &inputTurntableSynchedVideo,
  SettingsForOneBatch &aBatch);

void NextionEndCommand()
{ Nextion.print(F("\xFF\xFF\xFF")); }

SettingsForOneBatch getSystemDefaultSettings() {
  SettingsForOneBatch systemDefaultSettings;
  strcpy(systemDefaultSettings.startupPanel,"t");
  systemDefaultSettings.carriageSettings.stepsize = stepsPerRevolution * stepmultiplier;  // carriage's default step size is 1 full revolution
  systemDefaultSettings.carriageSettings.track1stop = stepsPerRevolution * stepmultiplier * 6;
  systemDefaultSettings.carriageSettings.track2stop = systemDefaultSettings.carriageSettings.track1stop * 2;
  systemDefaultSettings.carriageSettings.track3stop = systemDefaultSettings.carriageSettings.track1stop * 3;

  // Based on values actually in use on 4/28/25
  systemDefaultSettings.carriageSettings.stepsize = 100L;
  systemDefaultSettings.carriageSettings.track1stop = 9452L;
  systemDefaultSettings.carriageSettings.track2stop = 60306L;
  systemDefaultSettings.carriageSettings.track3stop = 110369L;
  systemDefaultSettings.carriageSettings.speed = 3200;
  systemDefaultSettings.carriageSettings.accel = 200;
  
  // Default values for use in FullStepMode
  // They are modified later, depending on the step mode we choose later.
  
  
  //  taileast should be exactly 400 from headeast
  //  tailwest should be exactly 400 from headwest
  //                                        no microstop // 32 microsteps
  systemDefaultSettings.turntableSettings.tailEastStop = 67L; // 2144
  systemDefaultSettings.turntableSettings.tailWestStop = 500L;  //16010
  systemDefaultSettings.turntableSettings.headEastStop = 467L;  // 14944
  systemDefaultSettings.turntableSettings.headWestStop = 100L;  // 3210
  systemDefaultSettings.turntableSettings.stepsize = 10L;
  // Default values for use in FullStepMode
  // They are modified later, depending on the step mode we choose later.
  //                                                          no-micro  // 32 microsteps
  systemDefaultSettings.turntableSettings.speed = 15; //480;
  systemDefaultSettings.turntableSettings.accel = 2; //64;
  
  
  // set the multiplier and adjust some default settings accordingly
  
  
  systemDefaultSettings.turntableSettings.speed =   
    systemDefaultSettings.turntableSettings.speed * stepMultiplierTurn;
  systemDefaultSettings.turntableSettings.accel =   
    systemDefaultSettings.turntableSettings.accel * stepMultiplierTurn;
    
  
  systemDefaultSettings.turntableSettings.tailEastStop  =
    systemDefaultSettings.turntableSettings.tailEastStop * stepMultiplierTurn;
  systemDefaultSettings.turntableSettings.tailWestStop =
    systemDefaultSettings.turntableSettings.tailWestStop * stepMultiplierTurn;
  systemDefaultSettings.turntableSettings.headEastStop =
    systemDefaultSettings.turntableSettings.headEastStop * stepMultiplierTurn;
  systemDefaultSettings.turntableSettings.headWestStop =
    systemDefaultSettings.turntableSettings.headWestStop * stepMultiplierTurn;

  
  // build the default SynchedVideoTurntable instances to look like this.
  // SynchedVideoTurntable starterTurntableSynchedVideo[] = {
  //   {"3", "35", "cw", -1L, 1, 1, 100},
  //   {"3", "8",  "cw", -1L, 1, 1, 100},
  //   {"3", "9",  "cw", -1L, 1, 1, 100},
  //   {"3", "95", "cw", -1L, 1, 1, 100},
  //   {"3", "2",  "cw", -1L, 1, 1, 100},
  //   {"3", "3",  "cw", -1L, 1, 1, 100},

  //   {"3", "2",  "ccw", -1L, 1, 1, 100},
  //   {"3", "95", "ccw", -1L, 1, 1, 100},
  //   {"3", "9",  "ccw", -1L, 1, 1, 100},
  //   {"3", "8",  "ccw", -1L, 1, 1, 100},
  //   {"3", "35", "ccw", -1L, 1, 1, 100},
  //   {"3", "3",  "ccw", -1L, 1, 1, 100},

  //   {"95", "2",  "cw", -1L, 1, 1, 100},
  //   {"95", "3",  "cw", -1L, 1, 1, 100},
  //   {"95", "35", "cw", -1L, 1, 1, 100},
  //   {"95", "8",  "cw", -1L, 1, 1, 100},
  //   {"95", "9",  "cw", -1L, 1, 1, 100},
  //   {"95", "95", "cw", -1L, 1, 1, 100},

  //   {"95", "9",  "ccw", -1L, 1, 1, 100},
  //   {"95", "8",  "ccw", -1L, 1, 1, 100},
  //   {"95", "35", "ccw", -1L, 1, 1, 100},
  //   {"95", "3",  "ccw", -1L, 1, 1, 100},
  //   {"95", "2",  "ccw", -1L, 1, 1, 100},
  //   {"95", "95", "ccw", -1L, 1, 1, 100}
  // };

  for (int ii=0; ii<=11; ii++){
    strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].end,"3");
  }
  for (int ii=12; ii<=23; ii++){
    strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].end,"95");
  }
  for (int ii=0; ii<=5; ii++){
    strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].direction,"cw");
  }
  for (int ii=6; ii<=11; ii++){
    strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].direction,"ccw");
  }
  for (int ii=12; ii<=17; ii++){
    strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].direction,"cw");
  }
  for (int ii=18; ii<=23; ii++){
    strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].direction,"ccw");
  }

  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[0].start,"35");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[1].start,"8");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[2].start,"9");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[3].start,"95");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[4].start,"2");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[5].start,"3");

  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[6].start,"2");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[7].start,"95");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[8].start,"9");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[9].start,"8");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[10].start,"35");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[11].start,"3");

  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[12].start,"2");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[13].start,"3");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[14].start,"35");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[15].start,"8");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[16].start,"9");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[17].start,"95");

  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[18].start,"9");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[19].start,"8");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[20].start,"35");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[21].start,"3");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[22].start,"2");
  strcpy(systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[23].start,"95");

  for (int ii=0; ii<=23; ii++){
    systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].stepCount = -1L;
    systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].accel = 1;
    systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].speed = 1;
    systemDefaultSettings.turntableSettings.aTurntableSynchedVideo[ii].videoDis = 100;
  }

  // build the default SynchedVideoCarriage instances to look like this:
  // SynchedVideoCarriage starterCarriageSynchedVideo[] = {
  //   {"1", "0", -1L, 1, 1, 100},
  //   {"2", "0", -1L, 1, 1, 100},
  //   {"3", "0", -1L, 1, 1, 100},
  //   {"2", "1", -1L, 1, 1, 100},
  //   {"3", "1", -1L, 1, 1, 100},
  //   {"1", "2", -1L, 1, 1, 100},
  //   {"3", "2", -1L, 1, 1, 100},
  //   {"2", "3", -1L, 1, 1, 100},
  //   {"1", "3", -1L, 1, 1, 100}
  // };
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[0].end,"1");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[0].start,"0");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[1].end,"2");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[1].start,"0");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[2].end,"3");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[2].start,"0");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[3].end,"2");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[3].start,"1");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[4].end,"3");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[4].start,"1");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[5].end,"1");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[5].start,"2");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[6].end,"3");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[6].start,"2");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[7].end,"2");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[7].start,"3");

  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[8].end,"1");
  strcpy(systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[8].start,"3");

  for (int ii=0; ii<=8; ii++){
    systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[ii].stepCount = -1L;
    systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[ii].accel = 1;
    systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[ii].speed = 1;
    systemDefaultSettings.carriageSettings.aCarriageSynchedVideo[ii].videoDis = 100;
  }
  return systemDefaultSettings;
}

void START_COMMON_ROUTINES() {}

void str_commas(char *srcAndDest, int sizeOfSrcAndDest) {  
  // assumes srcAndDest contains something like:  123  or 1234 or -123 or -1234 or 1234567
  // replaces the examples above with:  123   1,234   -123  -1,234  1,234,567
  // if input string can't hold the entire result, the input string is left unchanged
  if ((strlen(srcAndDest) == 0) || (strlen(srcAndDest) > MAX_STRING_LENGTH)){ return; }
  int sourceLen = strlen(srcAndDest);
  int temp = sourceLen -1;
  if (srcAndDest[0] == '-') {
    temp = temp - 1;
  }
  int commaCnt = temp / 3; 
  int destLen = sourceLen + commaCnt;
  if (destLen + 1 > sizeOfSrcAndDest) { return; }  // srcAndDest is too small to contain all characters plus the required str_commas
  memset(sbuf, '\0', sizeof(sbuf)); // clear the temporary answer area
  int targetIndex = destLen - 1;
  int sourceIndex = sourceLen - 1;
  int digitCount = 0;
  while (sourceIndex > -1) {
    if (digitCount == 3 && srcAndDest[sourceIndex] != '-') {
      sbuf[targetIndex] = ',';
      targetIndex--;
      digitCount = 0;
    }
    sbuf[targetIndex] = srcAndDest[sourceIndex];
    digitCount++;
    targetIndex--;
    sourceIndex--;
  }
  sbuf[destLen] = '\0'; 
  strncpy(srcAndDest, sbuf, strlen(sbuf)+1);
}

void convert_steps_to_text( long stepCount, char dest[15]) 
{
  char temp[15];
  strcpy_P(sbuf, (char *)pgm_read_word(&(st[67])));
  sprintf(temp, sbuf, stepCount);
  str_commas(temp, sizeof(temp));
  strcpy(dest, temp);
}

void send_numbered_msg_to_tMsg(int msgNumber) {
  strcpy_P(sbuf, (char *)pgm_read_word(&(st[msgNumber])));  
  Nextion.print(F("tMsg.txt=\"")); Nextion.print(sbuf); 
  Nextion.print(F("\"")); NextionEndCommand(); 
  Nextion.print(F("tmMsg.en=1")); 
  NextionEndCommand();  // enable the message timer to auto-erase
}

void send_text_msg_to_tMsg(char *theText) {
  Nextion.print(F("tMsg.txt=\"")); Nextion.print(theText); Nextion.print(F("\"")); NextionEndCommand(); 
  Nextion.print(F("tmMsg.en=1")); NextionEndCommand();  // enable the message timer to auto-erase
}

long calculate_TurntableVideo_stepCount(SynchedVideoTurntable &aTurntableSynchVideoInstance) {
  char end[3];
  char start[3];
  char direction[5];
  strcpy(end, aTurntableSynchVideoInstance.end);
  strcpy(start, aTurntableSynchVideoInstance.start);
  strcpy(direction, aTurntableSynchVideoInstance.direction);
  if (strcmp(end,start) ==0){   // if start = end then stepcount = 0, get out
      return 0;
  }
  
  long endPosition;
  if (strcmp(end, "3") == 0)  { endPosition = 
    unSavedSettings.turntableSettings.tailEastStop; }
  else { endPosition = unSavedSettings.turntableSettings.tailWestStop; }
  
  long startposition;
  if (strcmp(start,"2") == 0) { startposition = 0; }
  if (strcmp(start,"3") == 0) { startposition = 
    unSavedSettings.turntableSettings.tailEastStop;}
  if (strcmp(start,"35") == 0) { startposition = 
    unSavedSettings.turntableSettings.headWestStop;}
  if (strcmp(start,"8") == 0) { startposition = (stepsPerTurntableRevolution / 2);}      
  if (strcmp(start,"9") == 0) { startposition = 
    unSavedSettings.turntableSettings.headEastStop;} 
  if (strcmp(start,"95") == 0) { startposition = 
    unSavedSettings.turntableSettings.tailWestStop;}
  
  long delta = 0;
  if (strcmp(direction,"cw") == 0 ) { 
    if (startposition > endPosition) { delta = (stepsPerTurntableRevolution - startposition + endPosition); }
    else { delta = endPosition - startposition; }
  }
  else {
    if (endPosition > startposition) {
      delta = -1 * (stepsPerTurntableRevolution - endPosition + startposition);
    }    
    else { delta = -1 * (startposition - endPosition);
    }
  }
  if (delta < 0) { delta = (-1) * delta;}
  return delta;
}

bool is_one_turntable_SynchVideo_needed(SynchedVideoTurntable &aTurntableSynchVideoInstance,
  SettingsForOneBatch &aBatch) {
  bool ans = false;
  long calculatedStepCount = calculate_TurntableVideo_stepCount(aTurntableSynchVideoInstance);
  if (aTurntableSynchVideoInstance.accel !=
      aBatch.turntableSettings.accel ||
      
      aTurntableSynchVideoInstance.speed !=
      aBatch.turntableSettings.speed ||

      aTurntableSynchVideoInstance.stepCount != 
        calculatedStepCount ) {
          ans = true;
  }
  return ans;  
}

bool is_any_turntable_SynchVideo_needed() {
  bool ans = false;
  for (int ii=0; ii<=23; ii++){
    if (is_one_turntable_SynchVideo_needed(
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii],
      unSavedSettings)) { ans = true; }
  }
  return ans;  
}

long calculate_CarriageVideo_stepCount(SynchedVideoCarriage &aCarriageSynchVideoInstance,
  SettingsForOneBatch &aBatch) {
  
  char end[2] = " ";
  char start[2] = " ";
  strcpy(end, aCarriageSynchVideoInstance.end);
  strcpy(start, aCarriageSynchVideoInstance.start);

  // if start = end then stepcount = 0, get out
  if (strcmp(end,start) ==0){
    return 0;
  }
  long ans = 0;
  if ((strcmp(end,"1") == 0) && strcmp(start, "0") == 0) {
    ans = aBatch.carriageSettings.track1stop;
  }
  else {
    if ((strcmp(end,"2") == 0) && strcmp(start, "0") == 0) {
    ans = aBatch.carriageSettings.track2stop;
    }  
    else {
      if (strcmp(end,"3") == 0 && strcmp(start, "0") == 0) {
        ans = aBatch.carriageSettings.track3stop;
      }
      else {
        if ((strcmp(end,"2") == 0 && strcmp(start, "1") == 0)
          || (strcmp(end,"1") == 0 && strcmp(start, "2") == 0)) {
          ans = aBatch.carriageSettings.track2stop - aBatch.carriageSettings.track1stop;
        }
        else {
          if ((strcmp(end,"3") == 0 && strcmp(start, "1") == 0)
            || (strcmp(end,"1") == 0 && strcmp(start, "3") == 0)) {
            ans = aBatch.carriageSettings.track3stop - aBatch.carriageSettings.track1stop;
          }
          else {
             if ((strcmp(end,"3") == 0 && strcmp(start, "2") == 0)
              || (strcmp(end,"2") == 0 && strcmp(start, "3") == 0)) {
              ans = aBatch.carriageSettings.track3stop - aBatch.carriageSettings.track2stop;
            }
          }
        }
      }
    }
  }
  return ans;
}

bool is_one_carriage_SynchVideo_needed(SynchedVideoCarriage &aCarriageSynchVideoInstance,
  SettingsForOneBatch &aBatch) {
  bool ans = false;
  if (aCarriageSynchVideoInstance.accel !=
      aBatch.carriageSettings.accel ||
      
      aCarriageSynchVideoInstance.speed !=
      aBatch.carriageSettings.speed ||

      aCarriageSynchVideoInstance.stepCount !=
      calculate_CarriageVideo_stepCount(aCarriageSynchVideoInstance, aBatch))
      {
        ans = true;
      }
  return ans;  
}

bool is_any_carriage_SynchVideo_needed(SettingsForOneBatch &aBatch) {
  bool ans = false;
  
  for (int ii=0; ii<=8; ii++){
    if (is_one_carriage_SynchVideo_needed(aBatch.carriageSettings.aCarriageSynchedVideo[ii], 
      aBatch)) { ans = true; }
  }
  return ans;
}

bool is_exclamation_needed() {
  bool ans = false;
  if (unSavedSettings.turntableSettings.headEastStop !=
    onDiskSettings.savedSettings.turntableSettings.headEastStop ||
    unSavedSettings.turntableSettings.headWestStop !=
    onDiskSettings.savedSettings.turntableSettings.headWestStop ||
    unSavedSettings.turntableSettings.stepsize !=
    onDiskSettings.savedSettings.turntableSettings.stepsize ||
    unSavedSettings.turntableSettings.tailEastStop !=
    onDiskSettings.savedSettings.turntableSettings.tailEastStop ||
    unSavedSettings.turntableSettings.tailWestStop !=
    onDiskSettings.savedSettings.turntableSettings.tailWestStop ||
    (strcmp(unSavedSettings.startupPanel,onDiskSettings.savedSettings.startupPanel) != 0) ||
    unSavedSettings.turntableSettings.speed !=
    onDiskSettings.savedSettings.turntableSettings.speed ||
    unSavedSettings.turntableSettings.accel !=
    onDiskSettings.savedSettings.turntableSettings.accel) {
      ans = true;
  }
  for (int ii=0; ii<=23; ii++){
    if (unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].accel !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[ii].accel ||
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].speed !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[ii].speed ||
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].stepCount !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[ii].stepCount ||
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].videoDis !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[ii].videoDis) {
        ans = true;
    }
  }
  for (int ii=0; ii<=23; ii++){
    if (is_one_turntable_SynchVideo_needed(
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii],
      unSavedSettings)) { ans = true; }
  }

  if (unSavedSettings.carriageSettings.track1stop !=
    onDiskSettings.savedSettings.carriageSettings.track1stop
    || unSavedSettings.carriageSettings.track2stop !=
    onDiskSettings.savedSettings.carriageSettings.track2stop
    || unSavedSettings.carriageSettings.track3stop !=
    onDiskSettings.savedSettings.carriageSettings.track3stop
    || unSavedSettings.carriageSettings.stepsize !=
    onDiskSettings.savedSettings.carriageSettings.stepsize
    || strcmp(unSavedSettings.startupPanel,onDiskSettings.savedSettings.startupPanel) != 0
    || unSavedSettings.carriageSettings.speed !=
    onDiskSettings.savedSettings.carriageSettings.speed 
    || unSavedSettings.carriageSettings.accel !=
    onDiskSettings.savedSettings.carriageSettings.accel 
    ) { ans = true;}
  
  for (int ii=0; ii<=8; ii++){
    if (unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].accel !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[ii].accel ||
      unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].speed !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[ii].speed ||
      unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].stepCount !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[ii].stepCount ||
      unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].videoDis !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[ii].videoDis) {
        ans = true;
    }
  }
  
  for (int ii=0; ii<=8; ii++){

    if (is_one_carriage_SynchVideo_needed(
      unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii], 
      unSavedSettings)) {
       ans = true; }
  }
 
  return ans;
}

void END_COMMON_ROUTINES() {}


void START_PGBASICS_ROUTINES() {}

void set_CurrentPosition_data_on_pgBasics() {
  char answer[15];
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(mystepper.currentPosition(), answer);
  Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  Nextion.print(F("\"")); NextionEndCommand();   
}

void set_track_pictures_on_pgBasics_using_tableinput(CarriagePicturePositionSet picAndPos) {
  Nextion.print(F("bTrack1.pic=")); Nextion.print(picAndPos.bTrack1Pic); NextionEndCommand();   
  Nextion.print(F("bTrack2.pic=")); Nextion.print(picAndPos.bTrack2Pic); NextionEndCommand();   
  Nextion.print(F("bTrack3.pic=")); Nextion.print(picAndPos.bTrack3Pic); NextionEndCommand();   
  Nextion.print(F("pRtTrack.pic=")); Nextion.print(picAndPos.pRtTrackPic); NextionEndCommand();   
  Nextion.print(F("bTrack1.y=")); Nextion.print(picAndPos.bTrack1YPosition); NextionEndCommand();   
  Nextion.print(F("bTrack2.y=")); Nextion.print(picAndPos.bTrack2YPosition); NextionEndCommand();   
  Nextion.print(F("bTrack3.y=")); Nextion.print(picAndPos.bTrack3YPosition); NextionEndCommand();   
}

void set_track_pictures_on_pgBasics() {
  long tempCurrPos = mystepper.currentPosition();
  alignedAt = -1;
  if (homeStopCarriage != 0) { // everything is unknown
    //set pics and positions according to values in first entry in aCarriagePicturePositionSet
    set_track_pictures_on_pgBasics_using_tableinput(aCarriagePicturePositionSet[0]);  
    Nextion.print(F("bTrack3.aph=65")); NextionEndCommand();  
    Nextion.print(F("bTrack2.aph=65")); NextionEndCommand();  
    Nextion.print(F("bTrack1.aph=65")); NextionEndCommand();  
    Nextion.print(F("bTrack3.txt=\"Track 3\"")); NextionEndCommand(); 
    Nextion.print(F("bTrack2.txt=\"Track 2\"")); NextionEndCommand(); 
    Nextion.print(F("bTrack1.txt=\"Track 1\"")); NextionEndCommand(); 
    // gray out align button
    Nextion.print(F("bAlign.pic=16")); NextionEndCommand();  
    Nextion.print(F("bAlign.pic2=16")); NextionEndCommand();
    isAlignGrayedOnBasics = true;
  }
  else {
    Nextion.print(F("bTrack3.aph=127")); NextionEndCommand();  
    Nextion.print(F("bTrack2.aph=127")); NextionEndCommand();  
    Nextion.print(F("bTrack1.aph=127")); NextionEndCommand();  
    Nextion.print(F("bTrack3.txt=\" \"")); NextionEndCommand(); 
    Nextion.print(F("bTrack2.txt=\" \"")); NextionEndCommand(); 
    Nextion.print(F("bTrack1.txt=\" \"")); NextionEndCommand(); 
  }
  if (homeStopCarriage == 0) {
    if (tempCurrPos == homeStopCarriage) { alignedAt = 0; }
    else {
      if (tempCurrPos == unSavedSettings.carriageSettings.track1stop) { alignedAt = 1;}
      else {
        if (tempCurrPos == unSavedSettings.carriageSettings.track2stop) { alignedAt = 2;}
        else {
          if (tempCurrPos == unSavedSettings.carriageSettings.track3stop) { alignedAt = 3;}
        }
      }
    }
  }
   if (alignedAt < 0) {
    //set pics and positions according to values in first entry in aCarriagePicturePositionSet
    set_track_pictures_on_pgBasics_using_tableinput(aCarriagePicturePositionSet[0]);  
    Nextion.print(F("bAlign.pic=16")); NextionEndCommand();  
    Nextion.print(F("bAlign.pic2=16")); NextionEndCommand();
    Nextion.print(F("bTrack3.aph=65")); NextionEndCommand();  
    Nextion.print(F("bTrack2.aph=65")); NextionEndCommand();  
    Nextion.print(F("bTrack1.aph=65")); NextionEndCommand();  
    Nextion.print(F("bTrack3.txt=\"Track 3\"")); NextionEndCommand(); 
    Nextion.print(F("bTrack2.txt=\"Track 2\"")); NextionEndCommand(); 
    Nextion.print(F("bTrack1.txt=\"Track 1\"")); NextionEndCommand(); 
    // gray out align button
    Nextion.print(F("bAlign.pic=16")); NextionEndCommand();  
    Nextion.print(F("bAlign.pic2=16")); NextionEndCommand();
    isAlignGrayedOnBasics = true;
    return;
  }
  else {
    Nextion.print(F("bTrack3.aph=127")); NextionEndCommand();  
    Nextion.print(F("bTrack2.aph=127")); NextionEndCommand();  
    Nextion.print(F("bTrack1.aph=127")); NextionEndCommand();  
    Nextion.print(F("bTrack3.txt=\" \"")); NextionEndCommand(); 
    Nextion.print(F("bTrack2.txt=\" \"")); NextionEndCommand(); 
    Nextion.print(F("bTrack1.txt=\" \"")); NextionEndCommand(); 
  }
 

  bool isAlignedAtFound = false;
  for (int j = 0; j <= 12; j++) 
  {
    if (aCarriagePicturePositionSet[j].alignedAt == alignedAt
      && aCarriagePicturePositionSet[j].selectedExit == carrSelectedExit) {
        set_track_pictures_on_pgBasics_using_tableinput(aCarriagePicturePositionSet[j]);
        isAlignedAtFound = true;
        Nextion.print(F("bAlign.pic=112")); NextionEndCommand();  
        Nextion.print(F("bAlign.pic2=113")); NextionEndCommand();
        isAlignGrayedOnBasics = false;
        break;
      }
  }
  if (!isAlignedAtFound) {
    set_track_pictures_on_pgBasics_using_tableinput(aCarriagePicturePositionSet[0]);
    Nextion.print(F("bAlign.pic=16")); NextionEndCommand();  
    Nextion.print(F("bAlign.pic2=16")); NextionEndCommand();
    isAlignGrayedOnBasics = true;
  }
}


void set_data_on_pgBasics() {
  if (!ispgBasicsStarted) {
    Nextion.print(F("vis bStart,1")); NextionEndCommand();     // show Start
  }
  else {
    Nextion.print(F("vis bStart,0")); NextionEndCommand();     // hide Start
  }
  set_CurrentPosition_data_on_pgBasics();
  set_track_pictures_on_pgBasics();
  if (is_exclamation_needed()) {
    Nextion.print(F("vis pExclamation,1")); NextionEndCommand(); }
  else { Nextion.print(F("vis pExclamation,0")); NextionEndCommand();  }
   
  Nextion.print(F("vis vCarriage,0")); NextionEndCommand();   // hide vCarriage
  
   // turn off Align button if aligned, else turn it on
  if ((alignedAt == carrSelectedExit && alignedAt > 0) || alignedAt == (-1) || carrSelectedExit == (-1)) {
      // gray out the "Align" button
      Nextion.print(F("bAlign.pic=16")); NextionEndCommand();  
      Nextion.print(F("bAlign.pic2=16")); NextionEndCommand();  
      isAlignGrayedOnBasics = true;
  }
  else {
      // set the "Align" button to available
      Nextion.print(F("bAlign.pic=112")); NextionEndCommand();  
      Nextion.print(F("bAlign.pic2=113")); NextionEndCommand();  
      isAlignGrayedOnBasics = false;
  }
  if (is_exclamation_needed()) {
    Nextion.print(F("vis pExclamation,1")); NextionEndCommand(); }
  else { Nextion.print(F("vis pExclamation,0")); NextionEndCommand();  }

}


void END_PGBASICS_ROUTINES() {}


void START_CARRIAGE_MOVEMENT_ROUTINES() {}

void hideVideoIfItIsTimeCarriage() {
  if (isMotorTurnStoppedCarriage && (isVideoTurnStoppedCarriage || carrSelectedExit < 1)) {
    set_data_on_pgBasics(); // set track pics to the right values
    Nextion.print(F("vis vCarriage,0")); NextionEndCommand();   // hide vCarriage
    Nextion.print(F("vCarriage.en=0")); NextionEndCommand();   // reset video on vCarriage to the beginning
    Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();   // allow other buttons to be pressed
  }
}

void check_carriage_movement_interrupted() {
  uint8_t parm1 =0;
  uint8_t parm2 =0;
  uint8_t len = 0;
  uint8_t cmd = 0;
  char start_char;
  unsigned long tmr_1;
  bool cmd_found;
  bool len_found;
  while(Nextion.available() > 0) 
  {
    start_char = Nextion.read();      // Create a local variable (start_char) read and store the first byte on it  
    if(start_char == '#') {
      len = 0;
      if(Nextion.available() > 0){ 
        len = Nextion.read();      // Create local variable (len) / read and store the value of the second byte
      }
      else {               // wait a bit for the next byte (len) to come in
        tmr_1 = millis();
        len_found = true;
        while(Nextion.available() == 0) {
          if ((millis() - tmr_1) > 100) {
            len_found = false;
            break;
          }
        }
        if (len_found) {
          len = Nextion.read();
        }
      }  
      tmr_1 = millis();
      cmd_found = true;
      while(Nextion.available() < len){ // Waiting for all the bytes that we declare with <len> to arrive              
        if((millis() - tmr_1) > 100){    // Waiting... But not forever...... 
          cmd_found = false;              // tmr_1 a timer to avoid the stack in the while loop if there is not any bytes on Serial
          break;                            
        }                                     
      }                                   
      if(cmd_found == true){            // So..., A command is found (bytes in Serial buffer egual or more than len)
        cmd = Nextion.read();  
        if (cmd == 'S') {
          parm1 = Nextion.read();
          parm2 = Nextion.read();
          if (parm1 == 0x02 && parm2 == 0x00) {  // this means carriage video has completed.
            isVideoTurnStoppedCarriage = true;
            hideVideoIfItIsTimeCarriage();  
          }
        }
      }
    }  
  }   
  return;
}

void move_carriage(long stepsToMove, char direction, long speed, long accel) {
  if (stepsToMove == 0) { return; }
  if (direction != 'U' && direction != 'D') {return;}
  int trueDirection = 1;  // 1 means UP and -1 means DOWN.  We multiply the number of steps by this value to denote direction
  if (direction == 'D') {trueDirection = -1;}
  if (trueDirection == 1) {
    long tempCalculatedFinalStep = stepsToMove + mystepper.currentPosition();
    if (tempCalculatedFinalStep > topStopCarriage)  {
      send_numbered_msg_to_tMsg(19);  // error:  would go past top stop.
      return;
    }
  } 
  if (trueDirection == -1) {
    if (stepsToMove > mystepper.currentPosition()) {
      send_numbered_msg_to_tMsg(23);  // error:  would go past parked stop
      return;
    }
  }
  long targetPosition = 0;
  char answer[15];
  if (trueDirection == 1) { targetPosition = mystepper.currentPosition() + stepsToMove; }
  else { targetPosition = mystepper.currentPosition() - stepsToMove;}
  
  mystepper.moveTo(targetPosition);  // Set new moveto position of Stepper
  delay(10);  // Wait a bit before moving the Stepper
  unsigned long timer1 = millis();
  
  mystepper.setMaxSpeed(speed);
  mystepper.setAcceleration(accel);
  Nextion.print(F("tCurrentPos.pco=65535")); NextionEndCommand();  
  while(mystepper.distanceToGo() != 0) {
    check_carriage_movement_interrupted();
    mystepper.run();  // Move Stepper 1 step
    if ((millis() - timer1) > 1000) // update some stuff every 1 second
    {
      memset (answer, '\0', sizeof(answer));
      convert_steps_to_text(mystepper.currentPosition(), answer);
      Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  Nextion.print(F("\"")); NextionEndCommand();  
      // set_track_pictures_on_pgBasics();
      timer1 = millis();
    }
  }
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(mystepper.currentPosition(), answer);
  Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  Nextion.print(F("\"")); NextionEndCommand();   
}      

void move_carriage(long stepsToMove, char direction) {
  move_carriage(stepsToMove, direction, unSavedSettings.carriageSettings.speed,
    unSavedSettings.carriageSettings.accel);  
}
/**
 * @brief moves carriage to a specific position.  Automatically
 * determines if the carriage needs to go up or down.
 * 
 * @param targetPosition location to move to.  
 * @param showVideo true = play video while the movement is happening.
 * false = don't play video.
 */
void move_to_position(long targetPosition, bool showVideo) {
  if (mystepper.currentPosition() == targetPosition) {return;}

  long stepsToMove = 0;
  char direction = 'U';
  if (mystepper.currentPosition() < targetPosition) {
    stepsToMove = targetPosition - mystepper.currentPosition();
  } else {
    stepsToMove = mystepper.currentPosition() - targetPosition;
    direction = 'D';
  }
  
  int videoIDCarriage = -1;  
  if (showVideo) {
    // Serial.print(F("alignedAttt=")); Serial.println(alignedAt);
    char fullVideoName[5]="";  // examples:  e1.0, e3.2
    if (carrSelectedExit > 0) {
      if (carrSelectedExit == 3) {
        strcat(fullVideoName,"e3.");
      }
      else {
        if (carrSelectedExit == 2) {
          strcat(fullVideoName,"e2.");
          }
        else {
          strcat(fullVideoName,"e1.");
        } 
      }
      if (alignedAt == 0) {strcat(fullVideoName,"0");} Serial.print(F("fullVideoName=0")); Serial.println(fullVideoName);
      if (alignedAt == 1) {strcat(fullVideoName,"1");} Serial.print(F("fullVideoName=1")); Serial.println(fullVideoName);
      if (alignedAt == 2) {strcat(fullVideoName,"2");} Serial.print(F("fullVideoName=2")); Serial.println(fullVideoName);
      if (alignedAt == 3) {strcat(fullVideoName,"3");} Serial.print(F("fullVideoName=3")); Serial.println(fullVideoName);
    }
    // Serial.print(F("fullVideoName=")); Serial.println(fullVideoName);
    int i = 0;
    i = 0;
    do {
      if (strcmp(videoNameToVideoIDCarriage[i], fullVideoName) == 0) {
        videoIDCarriage = i+12;  // full list of videos on nextion has 12 of the turntable
        // videos and then the carriage videos.  So we add 12 to skip over the 12 turntable videos
        break;
      }
      i++;
    }
    while (i < 9);
    // Serial.print(F("videoIDCarriage=")); Serial.println(videoIDCarriage);
    if (videoIDCarriage > 0) {
      Nextion.print(F("vCarriage.en=2")); NextionEndCommand();  // pause playing the video

      // clear the input buffer so we don't process anything that's already stacked up
      while(Nextion.available()){Nextion.read();}    
      Nextion.print(F("vCarriage.vid=")); Nextion.print(videoIDCarriage); // set videoID
      NextionEndCommand();  
      Nextion.print(F("vCarriage.dis=")); // set video speed

      long newVideoDis = 100;
      i=0;
      do {
        if (atoi(unSavedSettings.carriageSettings.aCarriageSynchedVideo[i].start) == alignedAt &&
          atoi(unSavedSettings.carriageSettings.aCarriageSynchedVideo[i].end) == carrSelectedExit &&
          unSavedSettings.carriageSettings.aCarriageSynchedVideo[i].videoDis > (-1))
        {  
          newVideoDis = unSavedSettings.carriageSettings.aCarriageSynchedVideo[i].videoDis; 
          break;  
        }
      i++;
      }
      while (i < 9);
      Nextion.print(newVideoDis);
      NextionEndCommand();

      Nextion.print(F("vis vCarriage,1")); NextionEndCommand();  // show vCarriage
      isMotorTurnStoppedCarriage = false;  // set state to "is running"
      isVideoTurnStoppedCarriage = false;  // set state to "is running"
      Nextion.print(F("vCarriage.en=1")); NextionEndCommand();  // start playing the video
      elapsedMotorRunCarriage = 0;
      startAlignMSCarriage = millis();
    }
    else {
      Nextion.print(F("vis vCarriage,0")); NextionEndCommand();  // hide vCarriage
    }
  }
  Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons
  move_carriage(stepsToMove, direction);

  if (videoIDCarriage > 0) {  // showVideo was true and we found a good video file
    isMotorTurnStoppedCarriage = true;
    elapsedMotorRunCarriage = millis() - startAlignMSCarriage;
    hideVideoIfItIsTimeCarriage();
  }
  else {
    Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons
  }
}

/**
 * @brief moves carriage to a specific position.  Automatically
 * determines if the carriage needs to go up or down.  Automatically
 * suppresses the playing of video while the movement is happening.
 * 
 * @param targetPosition location to move to.
 */
void move_to_position(long targetPosition) {
  move_to_position(targetPosition,false);
}

void move_to_stop(int stopNumber, bool showVideo) {
  switch(stopNumber){
    case 3: 
      {
        move_to_position(unSavedSettings.carriageSettings.track3stop, showVideo);      
      }
      break;
      case 2: 
      {
        move_to_position(unSavedSettings.carriageSettings.track2stop, showVideo);
      }
      break;
      case 1: 
      {
        move_to_position(unSavedSettings.carriageSettings.track1stop, showVideo);
      }
      break;
      case 0: 
      {
        move_to_position(homeStopCarriage, showVideo);
      }
      break;
  }
}

void move_to_stop(int stopNumber) {
  move_to_stop(stopNumber, false);
}


void bAlignCarr_Click() {
  if (!checkIsPgBasicsStartedTrue()) { return; }
  // do nothing if already aligned

  
  if ((alignedAt == carrSelectedExit && alignedAt > 0) 
    || alignedAt == (-1) 
    || carrSelectedExit == (-1)
    || homeStopCarriage != 0
    || isAlignGrayedOnBasics) { return;}
  
  if (carrSelectedExit == 3) {
    move_to_stop((int) 3, true);
    return;
  }
  if (carrSelectedExit == 2) {
    move_to_stop((int) 2, true);
    return;
  }
  if (carrSelectedExit == 1) {
    move_to_stop((int) 1, true);
    return;
  }
}


void END_CARRIAGE_MOVEMENT_ROUTINES() {}

void START_CARRIAGE_COMMON_ROUTINES() {}


void reHome() {
    /*
    locates the home stop and also sets top stop to the top stop max when we
    know we have a good home stop.
    the routine performs its own carriage movements as opposed to relying
    on the normal move_carriage routine.  It moves the carriage slowly and in single steps so as to allow the
    limit switches to fire and stop the carriage movement.  
  */
// Serial.println(F("at reHome"));
  // long homingSpeed = unSavedSettings.carriageSettings.speeds.homing.speed;
  // long homingAccel = unSavedSettings.carriageSettings.speeds.homing.accel;
  long speed = unSavedSettings.carriageSettings.speed;
  long accel = unSavedSettings.carriageSettings.accel;
  // set the home and top stops to values that will force no movement if left unchanged.
  // the track3stop should always be positive and if this is > topStop and that will disable all movement.
  homeStopCarriage = -2;
  topStopCarriage = -1;
  char answer[15];
  long reHomeStepSizeDir = 0;  
  // 1=move 1 step CCW (away from home, towards top); 
  //-1=move 1 step CW (towards home, away from top
  // The +/- direction is true because we've done a setPinsInverted(t,f,f) earlier;  Normally + means to CW
  // With the motor nearest the aisle and not the wall, CCW pushes the table away from the wall, which 
  // makes the stationary track encounter home, then track 1, then track 2, then track 3, then top stop

// DEV Only - used when you don't want to bother with doing the actual re-home

  // homeStopCarriage = 0;
  // topStopCarriage =  maxTopStop;
  // delay(10);
  // mystepper.setCurrentPosition(0);
  // mystepper.setMaxSpeed(speed);      
  // mystepper.setAcceleration(accel);  
  
  // memset (answer, '\0', sizeof(answer));
  // convert_steps_to_text(mystepper.currentPosition(), answer);
  // Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  Nextion.print(F("\"")); NextionEndCommand();   
  // send_numbered_msg_to_tMsg(18);  // Re-home complete...
  // carrSelectedExit = 1;  // select track 1 as the default exit track
  // return;

  //  END DEV Only


  Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons
  mystepper.setCurrentPosition(0);
  mystepper.setMaxSpeed(speed);      // Set Max Speed of Stepper (Slower to get better accuracy)
  mystepper.setAcceleration(accel);  // Set Acceleration of Stepper
  // Serial.println("Stepper is Homing . . . . . . . . . . . ");
  send_numbered_msg_to_tMsg(61);  // Stepper is homing...
  // unsigned long timer1 = millis();
  Nextion.print(F("tCurrentPos.pco=65535")); NextionEndCommand();  
  if (digitalRead(homeCarriagePin)) {  // we are at or past the home limit switch;  move away until the switch goes off
    // Serial.println(F("initial position is at or past the home limit switch; moving towards topstop (CCW) until it goes off"));
    mystepper.setCurrentPosition(0);
    mystepper.setMaxSpeed(speed);      // Set Max Speed of Stepper (Slower to get better accuracy)
    mystepper.setAcceleration(accel);  // Set Acceleration of Stepper
    reHomeStepSizeDir = 1;
    while (digitalRead(homeCarriagePin)) {
      mystepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
      reHomeStepSizeDir++;  // Increase by 1 for next move if needed
      mystepper.run();  // Start moving the stepper
    }
    delay(10);
  }
  

  // we are now definitely between the 2 stops.  We can safely move "backwards" until we hit the home stop switch
  // Serial.println(F("positioned safely between the 2 stops; wait 10 ms"));
  delay(10);
  mystepper.setCurrentPosition(0);
  mystepper.setMaxSpeed(speed);      // Set Max Speed of Stepper (Slower to get better accuracy)
  mystepper.setAcceleration(accel);  // Set Acceleration of Stepper
  reHomeStepSizeDir = -1;
  // timer1 = millis();
  // Serial.println(F("starting to move backwards (CW) until we hit home"));
  send_numbered_msg_to_tMsg(63);  // Locating the home stop...
  while (!digitalRead(homeCarriagePin)) {  
    mystepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
    reHomeStepSizeDir--;  // Decrease by 1 for next move if needed
    mystepper.run();  // Start moving the stepper
  }
  delay(10); // back off (by going FORWARD) 1 step at a time until the home switch turn off
  mystepper.setCurrentPosition(0);
  mystepper.setMaxSpeed(speed);      // Set Max Speed of Stepper (Slower to get better accuracy)
  mystepper.setAcceleration(accel);  // Set Acceleration of Stepper
  delay(50);
  reHomeStepSizeDir = 1;
  while (digitalRead(homeCarriagePin)) {  // keep moving if the home switch is ON
    mystepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
    reHomeStepSizeDir++;
    mystepper.run(); 
  }

  homeStopCarriage = 0;
  topStopCarriage =  maxTopStop;
  delay(10);
  mystepper.setCurrentPosition(0);
  mystepper.setMaxSpeed(speed);      // Set Max Speed of Stepper (Slower to get better accuracy)
  mystepper.setAcceleration(accel);  // Set Acceleration of Stepper
  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(mystepper.currentPosition(), answer);
  Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  Nextion.print(F("\"")); NextionEndCommand();   
  send_numbered_msg_to_tMsg(18);  // Re-home complete...
  carrSelectedExit = 1;  // select track 1 as the default exit track
  Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to disable other buttons
}

void do_initializationCarriage() {
  unSavedSettings.carriageSettings = onDiskSettings.savedSettings.carriageSettings;
  reHome(); 
  set_CurrentPosition_data_on_pgBasics();
  // clear the input buffer as we can now safely accept more input
  while(Nextion.available()){Nextion.read();}
  set_data_on_pgBasics();
}

bool checkIsPgBasicsStartedTrue() {
  if (ispgBasicsStarted) { return true; }
  send_numbered_msg_to_tMsg(105);  // tells user that all controls are frozen
  return false;
}

void END_CARRIAGE_COMMON_ROUTINES() {}

void START_PGADJUSTMENTS_ROUTINES() {}


void htBasAdj_Click() {
  if (checkIsPgBasicsStartedTrue()) {
    Nextion.print(F("page pgAdjustments")); NextionEndCommand();   
  }
}

void htBasSettings_Click() {
  if (checkIsPgBasicsStartedTrue()) {
    Nextion.print(F("page pgSettings")); NextionEndCommand();   
  }  
}

bool is_CarriageSynchedVideo_A_Synch_Candidate(SynchedVideoCarriage &inputCarriageSynchedVideo,
  SettingsForOneBatch &aBatch) {
  long newStepCount = 
    calculate_CarriageVideo_stepCount(inputCarriageSynchedVideo, aBatch);
  if (aBatch.carriageSettings.speed !=
      inputCarriageSynchedVideo.speed 
      || aBatch.carriageSettings.accel !=
      inputCarriageSynchedVideo.accel 
      || newStepCount != inputCarriageSynchedVideo.stepCount
    ) { return true;}
  else { return false; }
  return false;
}



void bSynchVideoCarr_click() {
  int cntOfVideosNeedingModification = 0;
  int cntOfCurrentVideoBeingModified = 0;
  char ofMsg[40];
  char myBuffer[4];
  long tmpTargetPosition;


  long newStepCount = 0;
  long stepsToMove = 0;
  float tmp1 = 0;

  for (int jj=0; jj<=8; jj++){
    // for (int jj=0; jj<=2; jj++){
    if (is_CarriageSynchedVideo_A_Synch_Candidate(unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj], unSavedSettings)) {
      cntOfVideosNeedingModification = cntOfVideosNeedingModification + 1;
    }
  }
  if (cntOfVideosNeedingModification == 0) {return;}
  strcpy(ofMsg," of ");
  itoa(cntOfVideosNeedingModification, myBuffer,10);
  strcat(ofMsg, myBuffer);
  strcat(ofMsg, " video(s) being synched...");
  
  Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons
  digitalWrite(carriageEnbl,HIGH);
  // disable the carriage motor so we don't physically move the carriage
  // during synching
  for (int ii=0; ii<=8; ii++){
    // for (int ii=0; ii<=2; ii++){
    if (is_CarriageSynchedVideo_A_Synch_Candidate(unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii], unSavedSettings)) {
      newStepCount = calculate_CarriageVideo_stepCount(unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii], unSavedSettings);
      // Serial.print(F("newStepCount=")); Serial.println(newStepCount);
      Nextion.print(F("tMsg.txt=\"")); 
      strcpy(myBuffer,"");
      cntOfCurrentVideoBeingModified = cntOfCurrentVideoBeingModified + 1;
      Nextion.print(cntOfCurrentVideoBeingModified);
      Nextion.print(ofMsg);
      Nextion.print(F("\"")); NextionEndCommand(); 
           
        // run motor:  use newStepCount, speed, accel
        // capture elapsed time in ms
        // calculate videoDis
        // in aCarriageSynchedVideo[ii], store:
        // stepCount, speed, accel, videoDis

        // Build stepsToMove as either newStepCount or (-1) * newStepCount
        // We do this to minimize the number of times we have to go back to home.
        stepsToMove = newStepCount;
        if (newStepCount != 0) {
          mystepper.setMaxSpeed(unSavedSettings.carriageSettings.speed);
          mystepper.setAcceleration(unSavedSettings.carriageSettings.accel);
          if ((mystepper.currentPosition() + newStepCount) >= topStopCarriage) {  // we can't move up
            if ((mystepper.currentPosition() - newStepCount) >= 0) {  // we can move down.
              stepsToMove = newStepCount * (-1);
            }
            else {  // we can't move up or down
              move_to_position(0, false);  // move to home, don't show video
            }
          }
          tmpTargetPosition = mystepper.currentPosition() + stepsToMove;  // set target as set by stepsToMove
          elapsedMotorRunCarriage = 0;
          startAlignMSCarriage = millis();
          move_to_position(tmpTargetPosition, false);
          elapsedMotorRunCarriage = millis() - startAlignMSCarriage;
          delay(300);
          
          tmp1 = (float) aCarriageBaseVideo[ii].baseVideoMS / (float)elapsedMotorRunCarriage;
          unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].videoDis = tmp1 * 100;
        }
        else {
          unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].videoDis = 100;
        }
        unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].stepCount = newStepCount;  
        unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].speed = 
          unSavedSettings.carriageSettings.speed;
        unSavedSettings.carriageSettings.aCarriageSynchedVideo[ii].accel = 
          unSavedSettings.carriageSettings.accel;  
        set_data_on_pgAdjustments();  
     }
  }
  set_data_on_pgAdjustments();  
  // Serial.print(F("cnt was:"));  Serial.println(cntOfVideosNeedingModification);
  digitalWrite(carriageEnbl,LOW);  // re-enable carriage motor
  delay(500);  // give it time to settle down
  send_numbered_msg_to_tMsg(61);  // Stepper is homing...
  reHome();
  set_data_on_pgAdjustments();  
  Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons
}

void bSpeedUpCarriage_click() {
  long tmpSpeed = 0;
  if (strcmp(lastStopNameCarriage, "Speed") == 0) {
    tmpSpeed = unSavedSettings.carriageSettings.speed;    
  }
  else {
    tmpSpeed = unSavedSettings.carriageSettings.accel;
  }
  tmpSpeed = tmpSpeed + unSavedSettings.carriageSettings.stepsize;
  if (strcmp(lastStopNameCarriage, "Acceleration") == 0) {
    if (tmpSpeed > unSavedSettings.carriageSettings.speed) {  // msg:  accel can't be > speed
      send_numbered_msg_to_tMsg(0); 
      return;
    }
  }
  if (strcmp(lastStopNameCarriage, "Speed") == 0) {
    unSavedSettings.carriageSettings.speed = tmpSpeed;
  }
  else {
    if (strcmp(lastStopNameCarriage, "Acceleration") == 0) {
      unSavedSettings.carriageSettings.accel = tmpSpeed;
    }
  }
}    

void bSpeedDownCarriage_click() {
  long tmpSpeed = 0;
  if (strcmp(lastStopNameCarriage, "Speed") == 0) {
    tmpSpeed = unSavedSettings.carriageSettings.speed;    
  }
  else {
    tmpSpeed = unSavedSettings.carriageSettings.accel;
  }
  tmpSpeed = tmpSpeed - unSavedSettings.carriageSettings.stepsize;
  if (strcmp(lastStopNameCarriage, "Acceleration") == 0) {
    if (tmpSpeed < 0 ) {  // msg:  accel can't be < 0
      send_numbered_msg_to_tMsg(1); 
      return;
    }
  }
  if (strcmp(lastStopNameCarriage, "Speed") == 0) {
    if (tmpSpeed < 0 ) {  // msg:  speed can't be < 0
      send_numbered_msg_to_tMsg(2); 
      return;
    }
  }
  if (strcmp(lastStopNameCarriage, "Acceleration") == 0) {
    if (tmpSpeed > unSavedSettings.carriageSettings.speed) {  // msg:  accel can't be > speed
      send_numbered_msg_to_tMsg(0); 
      return;
    }
  }
  if (strcmp(lastStopNameCarriage, "Speed") == 0) {
    if (tmpSpeed < unSavedSettings.carriageSettings.accel) {  // msg:  accel can't be > speed
      send_numbered_msg_to_tMsg(0); 
      return;
    }
  }
  if (strcmp(lastStopNameCarriage, "Speed") == 0) {
    unSavedSettings.carriageSettings.speed = tmpSpeed;
  }
  else {
    if (strcmp(lastStopNameCarriage, "Acceleration") == 0) {
      unSavedSettings.carriageSettings.accel = tmpSpeed;
    }
  }
}

void bMoveToStop_click() {
  int targetStop = -1;
  if (strcmp(lastStopNameCarriage, "Track 1") == 0) { targetStop = 1; carrSelectedExit = 1; }
  if (strcmp(lastStopNameCarriage, "Track 2") == 0) { targetStop = 2; carrSelectedExit = 2; }
  if (strcmp(lastStopNameCarriage, "Track 3") == 0) { targetStop = 3; carrSelectedExit = 3; }
  if (strcmp(lastStopNameCarriage, "Home") == 0) { targetStop = 0; carrSelectedExit = -1;}
  if (targetStop >= 0) { move_to_stop(targetStop); }  
}

void bSettingNextCarriage_click() {
  // Serial.print(F("at setttingNextCarriage_click.  lastStopNameCarriage=")); Serial.println(lastStopNameCarriage);
  for (int ii=0; ii<=6; ii++){
    if (strcmp(lastStopNameCarriage, carriageStopNameStepDown[ii]) == 0) {
      strcpy(lastStopNameCarriage, carriageStopNameStepDown[ii+1]);
      break;
    }
  }
}

void bSettingPrevCarriage_click() {
  // Serial.print(F("at setttingPrevCarriage_click.  lastStopNameCarriage=")); Serial.println(lastStopNameCarriage);
  for (int ii=6; ii>=0; ii--){
    if (strcmp(lastStopNameCarriage, carriageStopNameStepDown[ii]) == 0) {
      strcpy(lastStopNameCarriage, carriageStopNameStepDown[ii-1]);
      break;
    }
  }
}

void bSetStopCarriage_click() {
  long currPos = mystepper.currentPosition();
  
  if (strcmp(lastStopNameCarriage,"Track 1") == 0) {
    if (currPos > homeStopCarriage &&
        currPos < unSavedSettings.carriageSettings.track2stop) {
          unSavedSettings.carriageSettings.track1stop = currPos;
          send_numbered_msg_to_tMsg(103);    
        }
    else {
      send_numbered_msg_to_tMsg(58);  // invalid vale
    }
    return;
  }
  if (strcmp(lastStopNameCarriage,"Track 2") == 0) {
    if (currPos > unSavedSettings.carriageSettings.track1stop &&
        currPos < unSavedSettings.carriageSettings.track3stop) {
          unSavedSettings.carriageSettings.track2stop = currPos;
          send_numbered_msg_to_tMsg(103);   
        }
    else {
      send_numbered_msg_to_tMsg(59);  // invalid vale
    }
    return;
  }
  if (strcmp(lastStopNameCarriage,"Track 3") == 0) {
     if (currPos > unSavedSettings.carriageSettings.track2stop &&
      currPos < topStopCarriage) {
        unSavedSettings.carriageSettings.track3stop = currPos;
        send_numbered_msg_to_tMsg(103);  
      }
    else {
      send_numbered_msg_to_tMsg(52);  // invalid vale
    }
    return;
  }
  if (strcmp(lastStopNameCarriage, "Startup Panel") ==0 ) {
        if (strcmp(unSavedSettings.startupPanel, "c") == 0) {
        strcpy(unSavedSettings.startupPanel, "t");
        }
        else {
          strcpy(unSavedSettings.startupPanel, "c");
        }
    send_numbered_msg_to_tMsg(103); 
  }
  
}

void cycleStepSize(int direction) {
  // direction is positive (or zero) for "make bigger";  negative for "make smaller"
  int trueDirection = 1;
  if (direction < 0) { trueDirection = -1; }
  // find current step size in the allSteps array
  int found = -1;
  for (int ptr = 0; ptr < 8; ptr++) {
    if (unSavedSettings.carriageSettings.stepsize == allSteps[ptr]) {
      found = ptr;
      break;
    }
  }
  if (found == -1) {found = 3;}  // point "found" to a reasonable default
  if (trueDirection == 1) {
    found--;  // we move "down" the array to "move bigger"
    if (found < 0) { found = 7;}
  }
  else {
    found++;  // we move "up" the array to "make smaller"
    if (found > 7) { found = 0; }
  }
  unSavedSettings.carriageSettings.stepsize = allSteps[found];
}

void htAdjBasicsPgAdjustments_click() {
  if (unSavedSettings.carriageSettings.track1stop == mystepper.currentPosition() ||
    unSavedSettings.carriageSettings.track2stop == mystepper.currentPosition() ||
    unSavedSettings.carriageSettings.track3stop == mystepper.currentPosition() ||
    mystepper.currentPosition() == 0) { // allow switching to another panel
      Nextion.print(F("page pgBasics")); NextionEndCommand();    
  }
  else {
    send_numbered_msg_to_tMsg(15); // move carriage to home or a stop first.
  }  
}

void htAdjSettingsPgAdjustments_click() {
  if (unSavedSettings.carriageSettings.track1stop == mystepper.currentPosition() ||
    unSavedSettings.carriageSettings.track2stop == mystepper.currentPosition() ||
    unSavedSettings.carriageSettings.track3stop == mystepper.currentPosition() ||
    mystepper.currentPosition() == 0) { // allow switching to another panel
      Nextion.print(F("page pgSettings")); NextionEndCommand();    
  }
  else {
    send_numbered_msg_to_tMsg(15); // move carriage to home or a stop first.
  }  
}

void set_data_on_pgAdjustments() {
  // Serial.print(F("top of pgAdjustments, laststopnamecarriage="));
  // Serial.println(lastStopNameCarriage);
  char answer[15]; 
  memset (answer, '\0', sizeof(answer));
  
  convert_steps_to_text(mystepper.currentPosition(), answer);
  Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer); Nextion.print(F("\"")); NextionEndCommand(); 
  convert_steps_to_text(topStopCarriage, answer);
  Nextion.print(F("tTopStop.txt=\"")); Nextion.print(answer); Nextion.print(F("\"")); NextionEndCommand(); 
  long current = -1;
  long saved = -1;
  long theDefault = -1;
  Nextion.print(F("tStopName.txt=\"")); Nextion.print(lastStopNameCarriage);  Nextion.print(F("\"")); NextionEndCommand();   
  if (strcmp(lastStopNameCarriage,"Track 1") == 0) {
    current = unSavedSettings.carriageSettings.track1stop;
    saved = onDiskSettings.savedSettings.carriageSettings.track1stop;
    theDefault = defaultSettings.carriageSettings.track1stop;
    Nextion.print(F("bMoveToStop.pic=60")); NextionEndCommand();   
    Nextion.print(F("bMoveToStop.pic2=59")); NextionEndCommand(); 
  }
  else {
    if (strcmp(lastStopNameCarriage,"Track 2") == 0) {
      current = unSavedSettings.carriageSettings.track2stop;
      saved = onDiskSettings.savedSettings.carriageSettings.track2stop;
      theDefault = defaultSettings.carriageSettings.track2stop;
      Nextion.print(F("bMoveToStop.pic=62")); NextionEndCommand();   
      Nextion.print(F("bMoveToStop.pic2=61")); NextionEndCommand();   

    }
    else {
      if (strcmp(lastStopNameCarriage,"Track 3") == 0) {
        current = unSavedSettings.carriageSettings.track3stop;
        saved = onDiskSettings.savedSettings.carriageSettings.track3stop;
        theDefault = defaultSettings.carriageSettings.track3stop;
        Nextion.print(F("bMoveToStop.pic=64")); NextionEndCommand();   
        Nextion.print(F("bMoveToStop.pic2=63")); NextionEndCommand();   

      }
      else {
        if (strcmp(lastStopNameCarriage,"Speed") == 0) {
          current = unSavedSettings.carriageSettings.speed;
          saved = onDiskSettings.savedSettings.carriageSettings.speed;
          theDefault = defaultSettings.carriageSettings.speed;
        }
        else {
          if (strcmp(lastStopNameCarriage,"Acceleration") == 0) {
            current = unSavedSettings.carriageSettings.accel;
            saved = onDiskSettings.savedSettings.carriageSettings.accel;
            theDefault = defaultSettings.carriageSettings.accel;
          } 
          else {
                current = 100;
          }
        }
      }
    }
  }
  
  char aPanelName[12] = "";
  if (current == -1){
    Nextion.print(F("tCurrentValue.txt=\" \"")); NextionEndCommand();   
    Nextion.print(F("tSavedValue.txt=\" \"")); NextionEndCommand();   
    Nextion.print(F("tDefaultValue.txt=\" \"")); NextionEndCommand();   
    Nextion.print(F("tSize.txt=\" \"")); NextionEndCommand();  
  } 
  else {
    if (strcmp(lastStopNameCarriage,"Startup Panel") == 0) {
      Nextion.print(F("tCurrentValue.txt=\"")); 
      if (strcmp(unSavedSettings.startupPanel,"c") == 0) { strcpy(aPanelName,"Carriage"); }
      else { strcpy(aPanelName,"Turntable");}
      Nextion.print(aPanelName);
      Nextion.print(F("\"")); NextionEndCommand();   
      Nextion.print(F("tSavedValue.txt=\"")); 
      if (strcmp(onDiskSettings.savedSettings.startupPanel,"c") == 0) { strcpy(aPanelName,"Carriage"); }
      else { strcpy(aPanelName,"Turntable");}
      Nextion.print(aPanelName);
      Nextion.print(F("\"")); NextionEndCommand();   
      Nextion.print(F("tDefaultValue.txt=\"")); 
      if (strcmp(defaultSettings.startupPanel,"c") == 0) { strcpy(aPanelName,"Carriage"); }
      else { strcpy(aPanelName,"Turntable");}
      Nextion.print(aPanelName);
      Nextion.print(F("\"")); NextionEndCommand();   
      
      Nextion.print(F("vis bSet,1")); NextionEndCommand();  
      Nextion.print(F("vis tSetExtra,1")); NextionEndCommand();  
      Nextion.print(F("vis bSpeedUp,0")); NextionEndCommand();  
      Nextion.print(F("vis bSpeedDown,0")); NextionEndCommand();  
      
      Nextion.print(F("vis tCustomLabel,0")); NextionEndCommand();  
      Nextion.print(F("vis tCustomFrame,0")); NextionEndCommand();  
      
      Nextion.print(F("vis tStepsLabel,0")); NextionEndCommand();  
      Nextion.print(F("vis tStepsFrame,0")); NextionEndCommand(); 
      Nextion.print(F("vis bStepDown,0")); NextionEndCommand();  
      Nextion.print(F("vis bStepUp,0")); NextionEndCommand();  
      Nextion.print(F("vis tSize,0")); NextionEndCommand();  
      
      Nextion.print(F("vis tActionLabel,0")); NextionEndCommand();  
      Nextion.print(F("vis tActionFrame,0")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveUp,0")); NextionEndCommand();  
      Nextion.print(F("vis bMoveDown,0")); NextionEndCommand();  
      

      Nextion.print(F("vis tStandardLabel,0")); NextionEndCommand(); 
      Nextion.print(F("vis tStandardFrame,0")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveToStop,0")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveToH,0")); NextionEndCommand(); 
        
      Nextion.print(F("vis bReHome,0")); NextionEndCommand(); 

    }
    else {
      if (strcmp(lastStopNameCarriage, "Speed") == 0 ||
        strcmp(lastStopNameCarriage, "Acceleration") == 0
        ) {
        
        memset (answer, '\0', sizeof(answer));
        convert_steps_to_text(current, answer);
        Nextion.print(F("tCurrentValue.txt=\"")); 
        Nextion.print(answer);
        Nextion.print(F("\"")); NextionEndCommand();   
        convert_steps_to_text(saved, answer);
        Nextion.print(F("tSavedValue.txt=\"")); 
        Nextion.print(answer);
        Nextion.print(F("\"")); NextionEndCommand();   
        convert_steps_to_text(theDefault, answer);
        Nextion.print(F("tDefaultValue.txt=\"")); 
        Nextion.print(answer);
        Nextion.print(F("\"")); NextionEndCommand(); 

        
        Nextion.print(F("vis bSet,0")); NextionEndCommand();  
        Nextion.print(F("vis tSetExtra,1")); NextionEndCommand();  
        Nextion.print(F("vis bSpeedUp,1")); NextionEndCommand();  
        Nextion.print(F("vis bSpeedDown,1")); NextionEndCommand();  
        
        Nextion.print(F("vis tCustomLabel,0")); NextionEndCommand();  
        Nextion.print(F("vis tCustomFrame,0")); NextionEndCommand();  
        
        Nextion.print(F("vis tStepsLabel,1")); NextionEndCommand();  
        Nextion.print(F("vis tStepsFrame,1")); NextionEndCommand(); 
        Nextion.print(F("vis bStepDown,1")); NextionEndCommand();  
        Nextion.print(F("vis bStepUp,1")); NextionEndCommand();  
        Nextion.print(F("vis tSize,1")); NextionEndCommand();  
        
        Nextion.print(F("vis tActionLabel,0")); NextionEndCommand();  
        Nextion.print(F("vis tActionFrame,0")); NextionEndCommand(); 
        Nextion.print(F("vis bMoveUp,0")); NextionEndCommand();  
        Nextion.print(F("vis bMoveDown,0")); NextionEndCommand();  
        

        Nextion.print(F("vis tStandardLabel,0")); NextionEndCommand(); 
        Nextion.print(F("vis tStandardFrame,0")); NextionEndCommand(); 
        Nextion.print(F("vis bMoveToStop,0")); NextionEndCommand(); 
        Nextion.print(F("vis bMoveToH,0")); NextionEndCommand(); 
          
        Nextion.print(F("vis bReHome,1")); NextionEndCommand();   

      }
      else {  // we are showing a stop
        memset (answer, '\0', sizeof(answer));
        convert_steps_to_text(current, answer);
        Nextion.print(F("tCurrentValue.txt=\"")); 
        Nextion.print(answer);
        Nextion.print(F("\"")); NextionEndCommand();   
        convert_steps_to_text(saved, answer);
        Nextion.print(F("tSavedValue.txt=\"")); 
        Nextion.print(answer);
        Nextion.print(F("\"")); NextionEndCommand();   
        convert_steps_to_text(theDefault, answer);
        Nextion.print(F("tDefaultValue.txt=\"")); 
        Nextion.print(answer);
        Nextion.print(F("\"")); NextionEndCommand(); 

        
        
        Nextion.print(F("vis bSet,1")); NextionEndCommand();  
        Nextion.print(F("vis tSetExtra,1")); NextionEndCommand();  
        Nextion.print(F("vis bSpeedUp,0")); NextionEndCommand();  
        Nextion.print(F("vis bSpeedDown,0")); NextionEndCommand();  
        
        Nextion.print(F("vis tCustomLabel,1")); NextionEndCommand();  
        Nextion.print(F("vis tCustomFrame,1")); NextionEndCommand();  
        
        Nextion.print(F("vis tStepsLabel,1")); NextionEndCommand();  
        Nextion.print(F("vis tStepsFrame,1")); NextionEndCommand(); 
        Nextion.print(F("vis bStepDown,1")); NextionEndCommand();  
        Nextion.print(F("vis bStepUp,1")); NextionEndCommand();  
        Nextion.print(F("vis tSize,1")); NextionEndCommand();  
        
        Nextion.print(F("vis tActionLabel,1")); NextionEndCommand();  
        Nextion.print(F("vis tActionFrame,1")); NextionEndCommand(); 
        Nextion.print(F("vis bMoveUp,1")); NextionEndCommand();  
        Nextion.print(F("vis bMoveDown,1")); NextionEndCommand();  
        

        Nextion.print(F("vis tStandardLabel,1")); NextionEndCommand(); 
        Nextion.print(F("vis tStandardFrame,1")); NextionEndCommand(); 
        Nextion.print(F("vis bMoveToStop,1")); NextionEndCommand(); 
        Nextion.print(F("vis bMoveToH,1")); NextionEndCommand(); 
          
        Nextion.print(F("vis bReHome,1")); NextionEndCommand();   
      }
    }
  }

  
  Nextion.print(F("tSize.txt=\"")); 
  Nextion.print(unSavedSettings.carriageSettings.stepsize); 
  Nextion.print(F("\"")); NextionEndCommand(); 
 
  Nextion.print(F("tSetExtra.txt=\""));
  if (strcmp(lastStopNameCarriage,"Startup Panel")==0 ) {
     if (strcmp(unSavedSettings.startupPanel, "c") == 0) {
        Nextion.print(F("         to\\rTURNTABLE"));
      }
      else { Nextion.print(F("        to\\rCARRIAGE"));
      }
  } 
  else {
    if (strcmp(lastStopNameCarriage, "Speed") == 0
      || strcmp(lastStopNameCarriage, "Acceleration") == 0)
      {
        Nextion.print(unSavedSettings.carriageSettings.stepsize);
      }
      else {
      Nextion.print(F("to the Current\\r    Position"));  
      }
  }
  
  Nextion.print(F("\"")); NextionEndCommand(); 
  
  if (is_any_carriage_SynchVideo_needed(unSavedSettings))
  {
    Nextion.print(F("vis bSynchVideo,1")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchLabel,1")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchFrame,1")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchLabel2,1")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchCnt,1")); NextionEndCommand(); 
    int cntOfVideosNeedingSynching = 0;
    for (int jj=0; jj<=8; jj++){
      if (is_CarriageSynchedVideo_A_Synch_Candidate(unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj], 
        unSavedSettings)) {
        cntOfVideosNeedingSynching = cntOfVideosNeedingSynching + 1;
      }
    }
    Nextion.print(F("tSynchCnt.txt=\""));
    Nextion.print(cntOfVideosNeedingSynching);
    Nextion.print(F("\"")); NextionEndCommand(); 
  }
  else {
    Nextion.print(F("vis bSynchVideo,0")); NextionEndCommand();
    Nextion.print(F("vis tSynchLabel,0")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchFrame,0")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchCnt,0")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchLabel2,0")); NextionEndCommand(); 
  }

  if (is_exclamation_needed()) {
    Nextion.print(F("vis pExclamation,1")); NextionEndCommand(); }
  else { Nextion.print(F("vis pExclamation,0")); NextionEndCommand();  }
}


void bMoveUp_click() {
  long targetPosition = 0;
  targetPosition = mystepper.currentPosition() + 
    unSavedSettings.carriageSettings.stepsize;
  if (targetPosition > topStopCarriage) {
    send_numbered_msg_to_tMsg(10);
  }  
  else {
    move_to_position(targetPosition);
    set_data_on_pgAdjustments();
    // send_numbered_msg_to_tMsg(11);
  }
}

void bMoveDown_click() {
  long targetPosition = 0;
  targetPosition = mystepper.currentPosition() - 
    unSavedSettings.carriageSettings.stepsize;
  if (targetPosition < homeStopCarriage) { 
    send_numbered_msg_to_tMsg(12);
  }  
  else {
    move_to_position(targetPosition);
    set_data_on_pgAdjustments();
  }
    
  
}

void END_PGADJUSTMENTS_ROUTINES() {}

void START_TURNTABLE_COMMON_ROUTINES() {}

bool isTurntableStopsOK() {
  if (homeStopTurntable != 0) {return 0;}
  if (homeStopTurntable < unSavedSettings.turntableSettings.tailEastStop &&
      unSavedSettings.turntableSettings.tailEastStop <
      unSavedSettings.turntableSettings.headWestStop &&
      unSavedSettings.turntableSettings.headWestStop <
      unSavedSettings.turntableSettings.headEastStop &&
      unSavedSettings.turntableSettings.headEastStop < 
      unSavedSettings.turntableSettings.tailWestStop &&
      unSavedSettings.turntableSettings.tailWestStop <
      stepsPerTurntableRevolution) {return 1;}
  else {return 0;}
}

bool isMagnetDetected() {
  // hallSensorPin:  1 = NO magnet present;  0 = magnet present
  if (digitalRead(hallSensorPin)) {return 0;}
  else {return 1;}
}

bool checkIsPgBasicsTurnStartedTrue() {
    if (ispgBasicsTurnStarted) { return true;}
  // we now know that pgBasicsStartTurn is false.  
  send_numbered_msg_to_tMsg(105);  // tells user that all controls are frozen
  return false;
}

long normalizeTurn(long aPosition) {  
/*
    returns the number of steps in aPosition 
    normalized to between 0 and stepsPerTurntableRevolution - 1 (inclusive)
    aPosition can be any value (negative or positive or 0)
    Examples assuming stepsPerTurntableRevolution = 10,000
    input   output
    -20,001 9,999
    -20,000 0
    -19,999 1
    -10,100 9,900
    -10,001 9,999
    -10,000 0
    -9,999  1
    -100    9,900
    -1      9,999
    0       0
    1       1
    100     100
    9,999   9,999
    10,000  0
    10,001  1
    10,100  100
    19,999  9,999
    20,000  0
    20,001  1
*/

  if (aPosition >= 0 && aPosition <= (stepsPerTurntableRevolution - 1)) {return aPosition;}
  long temp = 0;
  if (aPosition > 0) {  
  // then aPosition is > stepsPerTurntableRevolution
    temp = aPosition / stepsPerTurntableRevolution;  
    // yields number of full turntable revolutions because of integer division
    return (aPosition - (temp * stepsPerTurntableRevolution));
  }
  // then aPosition is < 0
  long positivePosition = -1 * aPosition;
  temp = positivePosition - (stepsPerTurntableRevolution * 
    (positivePosition / stepsPerTurntableRevolution));
  temp = stepsPerTurntableRevolution - temp;
  if (temp == stepsPerTurntableRevolution) {temp = 0;}
  return(temp);
}


void normalizespecial_turntable_currentpos_and_convert_to_text(char dest[15]) {
  memset (dest, '\0', 15);  // fixed answer size of 15
  long aPosition = normalizeTurn(turntablestepper.currentPosition());
  convert_steps_to_text(aPosition, dest);
}

void END_TURNTABLE_COMMON_ROUTINES() {}

void START_PGBASICSTURN_ROUTINES() {}


void set_track_pics() {
  // we store the last pictures used and only display them if they've changed
  // this speeds up the draw process, reduces the apparant short stopping of the turntable motor
  // and reduces flicker on the panel
  if (strcmp(currentbSlantedTrackPic, bSlantedTrackPic) != 0) {
    strcpy(currentbSlantedTrackPic,bSlantedTrackPic);
    Nextion.print(F("bSlantedTrack.pic="));  Nextion.print(currentbSlantedTrackPic); NextionEndCommand();   
  }
  if (strcmp(currentbStraightTrackPic,bStraightTrackPic) != 0) {
    strcpy(currentbStraightTrackPic, bStraightTrackPic);
    Nextion.print(F("bStraightTrack.pic="));  Nextion.print(currentbStraightTrackPic); NextionEndCommand();     
  }
  if (strcmp(currentNormalPic, NormalPic) != 0) {
    strcpy(currentNormalPic, NormalPic);
    strcpy(currentYellowPicL, YellowPicL);
    strcpy(currentYellowPicR, YellowPicR);
    Nextion.print(F("vNormalPic.val="));  Nextion.print(currentNormalPic); NextionEndCommand();     
    Nextion.print(F("pMain.pic="));  Nextion.print(currentNormalPic); NextionEndCommand();     
    Nextion.print(F("vYellowPicL.val="));  Nextion.print(currentYellowPicL); NextionEndCommand();     
    Nextion.print(F("vYellowPicR.val="));  Nextion.print(currentYellowPicR); NextionEndCommand();     
  }
  strcpy(currentLeftColor, leftColor);
  strcpy(currentSlantColor, slantColor);
  strcpy(currentRightColor, rightColor);
  strcpy(currentStraightColor, straightColor);
}


void set_track_pictures_on_pgBasicsTurn(bool forceRefresh) {
  if (!isTtblHomed) {
    strcpy(bSlantedTrackPic,"30"); // gray rails
    strcpy(bStraightTrackPic,"39"); //gray rails
    strcpy(NormalPic,"15"); //empty
    strcpy(YellowPicL,"15"); //empty
    strcpy(YellowPicR,"15"); //empty
    strcpy(leftColor,"");
    strcpy(rightColor,"");
    strcpy(slantColor,"");
    strcpy(straightColor,"");
    set_track_pics();
    // Serial.println("not ttblhomed");
    return;
  } 
  if (forceRefresh) {
    strcpy(currentbStraightTrackPic, "?");
    strcpy(currentbSlantedTrackPic, "?");
    strcpy(currentLeftColor, "?");
    strcpy(currentRightColor, "?");
    strcpy(currentSlantColor, "?");
    strcpy(currentStraightColor, "?");
    strcpy(currentNormalPic, "?");
    strcpy(currentYellowPicL, "?");
    strcpy(currentYellowPicR, "?");
  }
  normalizedTtblPos = normalizeTurn(turntablestepper.currentPosition());
   /*
    derive pointedSegmentName to be the segmentName that the tail end of the track is
    pointed at.  Derived from normalized turntable position.
  */
  if (normalizedTtblPos == 0) {
    strcpy(pointedSegmentName, "2");
  }
    else {
      if (normalizedTtblPos == unSavedSettings.turntableSettings.tailEastStop) {
        strcpy(pointedSegmentName, "3");
      }
      else {
        if (normalizedTtblPos == unSavedSettings.turntableSettings.headWestStop) {
          strcpy(pointedSegmentName, "3.5");
        } 
        else {
          if (normalizedTtblPos == unSavedSettings.turntableSettings.headEastStop) {
            strcpy(pointedSegmentName, "9");
          } 
          else {
            strcpy(pointedSegmentName, "9.5");
          }
        }
      }
    }
 
  // init all temp answers with a blank string that is the length of the answer -1
  // this gets a \0 in the last position of each answer.  this is needed because
  // strncpy later doesn't put one in.
  strcpy(zslantedPic,"  ");
  strcpy(zstraightPic, "  ");
  strcpy(zleftColor, " ");
  strcpy(zrightColor, " ");
  strcpy(zslantColor, " ");
  strcpy(zstraightColor, " ");
  strcpy(zhiddenTrueNormalPic,"   ");
  strcpy(zhiddenTrueYellowPicL,"   ");
  strcpy(zhiddenTrueYellowPicR,"   ");
  strcpy(zflippedFalseNormalPic,"   ");
  strcpy(zflippedFalseYellowPicL,"   ");
  strcpy(zflippedFalseYellowPicR,"   ");
  strcpy(zflippedTrueNormalPic,"   ");
  strcpy(zflippedTrueYellowPicL,"   ");
  strcpy(zflippedTrueYellowPicR,"   ");
  
  for (int j = 0; j <= 20; j++) {   // 19 is the highest index in aTrackPictureSetV2 array
    if (strcmp(aTrackPictureSetV2[j].segmentName,pointedSegmentName) == 0 &&
        strcmp(aTrackPictureSetV2[j].trackEnd,ttblSelectedTrackEnd) == 0 &&
        strcmp(aTrackPictureSetV2[j].exit,ttblSelectedExit) == 0) 
        { /* we found the correct entry in aTrackPictureSetV2.  "j" points to that entry
           and j is thus used to point to the corresponding entry in pictable */
        strcpy_P(sbuf, (char *)pgm_read_word(&(pictable[j]))); 
        strncpy(zslantedPic, sbuf, 2);
        strncpy(zstraightPic, sbuf+2, 2);
        strncpy(zhiddenTrueNormalPic, sbuf+4, 3);
        strncpy(zhiddenTrueYellowPicL, sbuf+7, 3);
        strncpy(zhiddenTrueYellowPicR, sbuf+10, 3);
        strncpy(zflippedFalseNormalPic, sbuf+13, 3);
        strncpy(zflippedFalseYellowPicL, sbuf+16, 3);
        strncpy(zflippedFalseYellowPicR, sbuf+19, 3);
        strncpy(zflippedTrueNormalPic, sbuf+22, 3);
        strncpy(zflippedTrueYellowPicL, sbuf+25, 3);
        strncpy(zflippedTrueYellowPicR, sbuf+28, 3);
        strncpy(zleftColor, sbuf+31, 1);
        strncpy(zrightColor, sbuf+32, 1);
        strncpy(zslantColor, sbuf+33, 1);
        strncpy(zstraightColor, sbuf+34, 1);
        strcpy(bSlantedTrackPic, zslantedPic);
        strcpy(bStraightTrackPic, zstraightPic);
        strcpy(leftColor, zleftColor);
        strcpy(rightColor, zrightColor);
        strcpy(slantColor, zslantColor);
        strcpy(straightColor, zstraightColor);
        if (!isShowLoco) {
          strcpy(NormalPic, zhiddenTrueNormalPic);
          strcpy(YellowPicL, zhiddenTrueYellowPicL);
          strcpy(YellowPicR, zhiddenTrueYellowPicR);
        }
        else {
          if (!isLocoFlipped) {  // then tailend of loco is on tailend of track
            strcpy(NormalPic, zflippedFalseNormalPic);
            strcpy(YellowPicL, zflippedFalseYellowPicL);
            strcpy(YellowPicR, zflippedFalseYellowPicR);
          }
          else {
            strcpy(NormalPic, zflippedTrueNormalPic);
            strcpy(YellowPicL, zflippedTrueYellowPicL);
            strcpy(YellowPicR, zflippedTrueYellowPicR);
          }
        }
        break;
      }
  }
  set_track_pics();

  // gray out CW and CCW buttons if aligned, else light them up
  if ((strcmp(currentLeftColor,"n") ==0 && strcmp(currentSlantColor,"n") == 0) ||
    (strcmp(currentRightColor,"n") ==0 && strcmp(currentStraightColor,"n") == 0)) {
      // gray out the buttons
      Nextion.print(F("bMoveCW.pic=77")); NextionEndCommand();  
      Nextion.print(F("bMoveCW.pic2=77")); NextionEndCommand();  
      Nextion.print(F("bMoveCCW.pic=80")); NextionEndCommand();  
      Nextion.print(F("bMoveCCW.pic2=80")); NextionEndCommand();  
      isAlignGrayedOnBasicsTurn = true;
    }
  else {
      // show the buttons as active
      Nextion.print(F("bMoveCW.pic=76")); NextionEndCommand();  
      Nextion.print(F("bMoveCW.pic2=74")); NextionEndCommand();  
      Nextion.print(F("bMoveCCW.pic=79")); NextionEndCommand();  
      Nextion.print(F("bMoveCCW.pic2=78")); NextionEndCommand();  
      isAlignGrayedOnBasicsTurn = false;
  }
}


void set_track_pictures_on_pgBasicsTurn() {
  set_track_pictures_on_pgBasicsTurn(false);
}


void set_CurrentPosition_data_on_pgBasicsTurn() {
  char answer[15];
  normalizespecial_turntable_currentpos_and_convert_to_text(answer);
  Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  
  Nextion.print(F("\"")); NextionEndCommand();   
}

void set_data_on_pgBasicsTurn(bool forceRefresh) {
  if (!ispgBasicsTurnStarted) {
    Nextion.print(F("vis bStart,1")); NextionEndCommand();     // show start
  }
  else {
    Nextion.print(F("vis bStart,0")); NextionEndCommand();     // hide start
  }
  
  Nextion.print(F("tSTrackEndVal.txt=\"")); 
  if (strcmp(ttblSelectedTrackEnd,"h") ==0) { Nextion.print(F("Head")); }
  else {Nextion.print(F("Tail"));}
  Nextion.print(F("\"")); NextionEndCommand();   

  Nextion.print(F("tSExitVal.txt=\"")); 
  if (strcmp(ttblSelectedExit,"e") ==0) { Nextion.print(F("East")); }
  else {Nextion.print(F("West"));}
  Nextion.print(F("\"")); NextionEndCommand();     
  
  Nextion.print(F("tCurrentPos.txt=\" \"")); NextionEndCommand(); 
  set_CurrentPosition_data_on_pgBasicsTurn();
  
  if (isShowLoco) {
    Nextion.print(F("bShowLoco.pic=194")); NextionEndCommand();   
    Nextion.print(F("bShowLoco.pic2=193")); NextionEndCommand();  
    Nextion.print(F("bFlipLoco.pic=192")); NextionEndCommand();   
    Nextion.print(F("bFlipLoco.pic2=191")); NextionEndCommand();  
  }
  else {
    Nextion.print(F("bShowLoco.pic=196")); NextionEndCommand();   
    Nextion.print(F("bShowLoco.pic2=195")); NextionEndCommand();   
    Nextion.print(F("bFlipLoco.pic=27")); NextionEndCommand();   
    Nextion.print(F("bFlipLoco.pic2=27")); NextionEndCommand();  
  }
  if (is_exclamation_needed()) {
    Nextion.print(F("vis pExclamation,1")); NextionEndCommand(); }
  else { Nextion.print(F("vis pExclamation,0")); NextionEndCommand();  }
  set_track_pictures_on_pgBasicsTurn(forceRefresh);
}

void set_data_on_pgBasicsTurn() {
  set_data_on_pgBasicsTurn(false);
}

void bShowLoco_click() {
  isShowLoco = !isShowLoco;
  if (isShowLoco) {
    Nextion.print(F("bShowLoco.pic=194")); NextionEndCommand();   
    Nextion.print(F("bShowLoco.pic2=193")); NextionEndCommand(); 
    Nextion.print(F("bFlipLoco.pic=192")); NextionEndCommand();   
    Nextion.print(F("bFlipLoco.pic2=191")); NextionEndCommand();  
  }
  else {
    Nextion.print(F("bShowLoco.pic=196")); NextionEndCommand();   
    Nextion.print(F("bShowLoco.pic2=195")); NextionEndCommand();   
    Nextion.print(F("bFlipLoco.pic=27")); NextionEndCommand();   
    Nextion.print(F("bFlipLoco.pic2=27")); NextionEndCommand();  
  }
  set_track_pictures_on_pgBasicsTurn();
}

void hideVideoIfItIsTimeTurn() {
  if (isMotorTurnStopped && isVideoTurnStopped) {
    set_data_on_pgBasicsTurn(); // set track pics to the right values
    // hide vTurnTable
    Nextion.print(F("vTurnTable.aph=0")); NextionEndCommand();   
    // reset video on vTurnTable to the beginning
    Nextion.print(F("vTurnTable.en=0")); NextionEndCommand();   
    // allow other buttons to be pressed
    Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();   
  }
}

void check_turntable_movement_interrupted() {
  uint8_t parm1 =0;   uint8_t parm2 =0;   uint8_t len = 0;   uint8_t cmd = 0;
  char start_char;
  unsigned long tmr_1;
  bool cmd_found;
  bool len_found;
  while(Nextion.available() > 0) 
  {
    start_char = Nextion.read();      
    if(start_char == '#') {
      len = 0;
      if(Nextion.available() > 0){ 
        len = Nextion.read();      
      }
      else {               // wait a bit for the next byte (len) to come in
        tmr_1 = millis();
        len_found = true;
        while(Nextion.available() == 0) {
          if ((millis() - tmr_1) > 100) {
            len_found = false;
            break;
          }
        }
        if (len_found) {
          len = Nextion.read();
        }
      }  
      tmr_1 = millis();
      cmd_found = true;
      while(Nextion.available() < len){ // Waiting for all the bytes to arrive
        if((millis() - tmr_1) > 100){    // Waiting... But not forever...... 
          cmd_found = false;   
          break;                            
        }                                     
      }                                   
      if(cmd_found == true){  // A command is found
        cmd = Nextion.read();  
        if (cmd == 'S') {
          parm1 = Nextion.read();
          parm2 = Nextion.read();
          if (parm1 == 0x01 && parm2 == 0x00) {  // turntable video has completed.
                isVideoTurnStopped = true;
                hideVideoIfItIsTimeTurn();   
                return; 
          }
        }
      }
    }  
  }   
  return;
}

void move_turntable(long stepsToMove) {  
  // stepsToMove is relative to the current position.  Positive is CW, negative is CCW.
  if (stepsToMove == 0) { return; }
  char answer[15];
  // sets new move position of Stepper. the move() function expects a RELATIVE value
  turntablestepper.move(stepsToMove); 
  delay(10);  // Wait a bit before moving the Stepper
  unsigned long timer1 = millis();

  turntablestepper.setMaxSpeed(unSavedSettings.turntableSettings.speed);
  turntablestepper.setAcceleration(unSavedSettings.turntableSettings.accel);
    
  while(turntablestepper.distanceToGo() != 0) {
    check_turntable_movement_interrupted();
    turntablestepper.run();  // Move Stepper into position.
    // Actually the called routine runs just 1 step and then returns control to me.
    // But the library keeps track of the position and speed.
    // By calling the .run() repeatedly quickly within a loop,
    // we get the desired motor motion.
    if ((millis() - timer1) > 1000) // every 1 second, update the main display
    {
      normalizespecial_turntable_currentpos_and_convert_to_text(answer);
      Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand(); 
      timer1 = millis();
    }
  }
  // move has ended.  update the current position one more time
  normalizespecial_turntable_currentpos_and_convert_to_text(answer);
  Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  
  Nextion.print(F("\"")); NextionEndCommand(); 
}

void move_to_positionTurn(long targetPosition, bool isCW) {  
  long normalizedCurrentPosition = normalizeTurn(turntablestepper.currentPosition());
  long normalizedTargetPosition = normalizeTurn(targetPosition);
  if (normalizedCurrentPosition == normalizedTargetPosition) {return;}
  long delta = 0;
  if (isCW) { 
    if (normalizedCurrentPosition > normalizedTargetPosition) {
      delta = (stepsPerTurntableRevolution 
        - normalizedCurrentPosition + normalizedTargetPosition);
    }
    else {
      delta = normalizedTargetPosition - normalizedCurrentPosition;
    }
  }
  else {
    if (normalizedTargetPosition > normalizedCurrentPosition) {
      delta = -1 * (stepsPerTurntableRevolution
         - normalizedTargetPosition + normalizedCurrentPosition);
    }    
    else {
      delta = -1 * (normalizedCurrentPosition - normalizedTargetPosition);
    }
  }
  move_turntable(delta);
}

void move_to_positionTurn(long targetPosition) {
    move_to_positionTurn(targetPosition, isbTravelDirCW);
  }  

// void reHomeTurn() {
//   // home position on the turntable is defined as when 
//   // you've just hit the detector when coming at the detector from the bottom
//   // ("bottom" is defined as coming from 3:00 to 2:00 CCW)
//   // we rotate the turntable CW and CCW one or more times at the normal and homing
//   // speeds to get a consistent reading of the home position in all cases
    
//   isTtblHomed = false;
//   long speed = unSavedSettings.turntableSettings.speeds.normal.speed;
//   long accel = unSavedSettings.turntableSettings.speeds.normal.accel;
//   long homingSpeed = unSavedSettings.turntableSettings.speeds.homing.speed;
//   long homingAccel = unSavedSettings.turntableSettings.speeds.homing.accel;
//   homeStopTurntable = -2;
//   long reHomeMoveTo = 0;
//   long reHomeStepSizeDir = 0;


//   // DEV Only

//   // Serial.println(F("at DEV Only rehometurn()"));
//   // isTtblHomed = true;
//   // delay(10); 
//   // homeStopTurntable = 0;
//   // turntablestepper.setCurrentPosition(0);
//   // turntablestepper.setMaxSpeed(speed);
//   // turntablestepper.setAcceleration(accel);
//   // strcpy(ttblSelectedExit, "e");
//   // strcpy(ttblSelectedTrackEnd, "t");
  

//   // send_numbered_msg_to_tMsg(18);
//   // return;

//   //  END DEV Only
  
//   send_numbered_msg_to_tMsg(61);  // stepper is rehoming...
//   turntablestepper.setCurrentPosition(0);
  
//   if (isMagnetDetected()) {  
//     // detector sees the magnet so we are starting out over the detector
//     // move CW at single step / homing speed until we are off the detector
//     turntablestepper.setCurrentPosition(0);
//     turntablestepper.setMaxSpeed(homingSpeed);
//     turntablestepper.setAcceleration(homingAccel);
//     reHomeStepSizeDir = 1;  // we move away from detector by going CW
//     while (isMagnetDetected()) {
//       turntablestepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
//       reHomeStepSizeDir++;  // Increase by 1 for next move if needed
//       turntablestepper.run();  // move stepper 1 step
//     }

//   // wait 0.5 seconds
//     delay(500);
   
//   // move CCW at single step / homing speed until we hit the detector
//     turntablestepper.setCurrentPosition(0);
//     turntablestepper.setMaxSpeed(homingSpeed);
//     turntablestepper.setAcceleration(homingAccel);
//     reHomeStepSizeDir = -1;
//     send_numbered_msg_to_tMsg(63);  // Locating the home stop...
//     while (!isMagnetDetected()) {  
//       turntablestepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
//       reHomeStepSizeDir--;  // Decrease by 1 for next move if needed
//       turntablestepper.run();  // move 1 step
//     }
//   }
//   else {
//     // detector does NOT initially see the magnet so we are starting out
//     // NOT over the detector
//     // so, move in the direction specified by isbTravelDirCW at normal speed
//     // until we hit the detector
    
    
//     turntablestepper.setCurrentPosition(0);  // also sets motor speed to 0
//     turntablestepper.setMaxSpeed(speed);
//     turntablestepper.setAcceleration(accel);
//     if (isbTravelDirCW) {
//       reHomeMoveTo = 2L;
//       reHomeStepSizeDir = 1;
//     }
//     else {
//       reHomeMoveTo = -2L;
//       reHomeStepSizeDir = -1;
//     }
//     // we set the MoveTo as 2 full revolutions away, in the proper direction to ensure
//     // that we eventually hit the detector.
//     reHomeMoveTo = reHomeMoveTo * stepsPerTurntableRevolution;
//     send_numbered_msg_to_tMsg(63);  // Locating the home stop...
//     turntablestepper.moveTo(reHomeMoveTo);  
//     while (!isMagnetDetected()) {  
//       turntablestepper.run();  // move 1 step.  do this until we hit the magnet
//       if (isMagnetDetected()) {
//         turntablestepper.stop();  // sets a new target to get us to stop as fast as possible
//         turntablestepper.runToPosition();  // run to the new position
//         break;
//       }
//     }
//     // if we were going CW to hit the detector then we have just hit it
//     // on the "top" of the detector
//     // OR
//     // we were going CCW to hit the detector then we have just hit it
//     // on the "bottom" of the detector

//     // Either way we do the following:
//     //    1. move CW at single step and homing speed until we aren't on it
//     //    2. wait 0.5 seconds
//     //    3. move CCW at single step and homing speed until we hit the detector

//     // First, move CW at homing speed until we are not on the detector
//     turntablestepper.setCurrentPosition(0);
//     turntablestepper.setMaxSpeed(homingSpeed);
//     turntablestepper.setAcceleration(homingAccel);
//     reHomeStepSizeDir = 1;
//     while (isMagnetDetected()) {  
//       turntablestepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
//       reHomeStepSizeDir++; // increase by 1 by next move if needed
//       turntablestepper.run();  // move 1 step
//     }
    
//     // Second, wait 0.5 seconds
//     delay(500);  // give it time to calm down.
  
//     // Third, move CCW at homing speed until we are on the detector
//     turntablestepper.setCurrentPosition(0);
//     reHomeStepSizeDir = -1;
//     while (!isMagnetDetected()) {  
//       turntablestepper.moveTo(reHomeStepSizeDir);  // Set the position to move to
//       reHomeStepSizeDir--; // decrease by 1 by next move if needed
//       turntablestepper.run();  // move 1 step
//     }
//   }
//   isTtblHomed = true;
//   homeStopTurntable = 0;
//   turntablestepper.setCurrentPosition(0);
//   turntablestepper.setMaxSpeed(speed);
//   turntablestepper.setAcceleration(accel);
//   strcpy(ttblSelectedExit, "e");
//   strcpy(ttblSelectedTrackEnd, "t");
//   send_numbered_msg_to_tMsg(18);
// }

void reHomeTurn(bool isCW) {  
  // isCW defines direction to rotate during re-home.  if true, use CW as the direction
  
  // home position on the turntable is defined as when 
  // you've just hit the detector when coming at the detector from the bottom
  // ("bottom" is defined as coming from 3:00 to 2:00 CCW)
  // we rotate the turntable CW and CCW one or more times at the normal and homing
  // speeds to get a consistent reading of the home position in all cases
    
  isTtblHomed = false;
  long speed = unSavedSettings.turntableSettings.speed;
  long accel = unSavedSettings.turntableSettings.accel;
  homeStopTurntable = -2;
  long reHomeMoveTo = 0;
 
  send_numbered_msg_to_tMsg(61);  // stepper is rehoming...
  turntablestepper.setCurrentPosition(0);
  turntablestepper.setMaxSpeed(speed);
  turntablestepper.setAcceleration(accel);
  if (isMagnetDetected()) {  
    // detector sees the magnet so we are starting out over the detector
    // move CW at single step / normal speed until we are off the detector
    turntablestepper.setCurrentPosition(0);
    turntablestepper.setMaxSpeed(speed);
    turntablestepper.setAcceleration(accel);
    reHomeMoveTo = 1;  // we move away from detector by going CW
    while (isMagnetDetected()) {
      turntablestepper.moveTo(reHomeMoveTo);  // Set the position to move to
      reHomeMoveTo++;  // Increase by 1 for next move if needed
      turntablestepper.run();  // move stepper 1 step
    }

  // wait 0.5 seconds
    delay(500);
   
  // move CCW at single step / normal speed until we hit the detector
    turntablestepper.setCurrentPosition(0);
    turntablestepper.setMaxSpeed(speed);
    turntablestepper.setAcceleration(accel);
    reHomeMoveTo = -1;
    send_numbered_msg_to_tMsg(63);  // Locating the home stop...
    while (!isMagnetDetected()) {  
      turntablestepper.moveTo(reHomeMoveTo);  // Set the position to move to
      reHomeMoveTo--;  // Decrease by 1 for next move if needed
      turntablestepper.run();  // move 1 step
    }
  }
  else {
    // detector does NOT initially see the magnet so we are starting out
    // NOT over the detector
    // so, move in the direction specified by isbTravelDirCW at normal speed
    // until we hit the detector
    
    turntablestepper.setCurrentPosition(0);  // also sets motor speed to 0
    turntablestepper.setMaxSpeed(speed);
    turntablestepper.setAcceleration(accel);
    send_numbered_msg_to_tMsg(63);  // Locating the home stop...
    // Serial.print("isCW=");Serial.println(isCW);
    if (isCW) {  // user wants to rehome by going clockwise
      reHomeMoveTo = 1;
      // rotate CW while magnet is not detected
      while (!isMagnetDetected()) { 
        turntablestepper.moveTo(reHomeMoveTo); // Set the position to move to
        reHomeMoveTo++; // increase by 1 for next move if needed
        turntablestepper.run();  // move 1 step 
      }
      // magnet is now detected.  rotate CW whil magnet is detected
      while (isMagnetDetected()) { 
        turntablestepper.moveTo(reHomeMoveTo); // Set the position to move to
        reHomeMoveTo++; // increase by 1 for next move if needed
        turntablestepper.run();  // move 1 step 
      }
      // magnet is now undetected.  rotate CCW while magnet is undetected
      // since we are changing direction and want to go 1 step
      // at a time, need to set current position to 0 
      delay(500);
      turntablestepper.setCurrentPosition(0);  // also sets motor speed to 0
      turntablestepper.setMaxSpeed(speed);
      turntablestepper.setAcceleration(accel);
      reHomeMoveTo = -1;
      while (!isMagnetDetected()) {  
        turntablestepper.moveTo(reHomeMoveTo);  // Set the position to move to
        reHomeMoveTo--;  // Decrease by 1 for next move if needed
        turntablestepper.run();  // move 1 step
      }
      // magnet has just hit the sensor when coming from 3:00 to 2:00
    }
    else {   // user wants to rehome by going counter-clockwise
      reHomeMoveTo = -1;
      // rotate CCW while the magnet is not detected
      while (!isMagnetDetected()) { 
        turntablestepper.moveTo(reHomeMoveTo); // Set the position to move to
        reHomeMoveTo--; // decrease by 1 for next move if needed
        turntablestepper.run();  // move 1 step 
      }
      // magnet is detected
      // rotate CW while the magnet is detected
      // since we are changing direction and want to go 1 step
      // at a time, need to set current position to 0
      delay(500);
      turntablestepper.setCurrentPosition(0);  // also sets motor speed to 0
      turntablestepper.setMaxSpeed(speed);
      turntablestepper.setAcceleration(accel);
      reHomeMoveTo = 1;
      while (isMagnetDetected()) { 
        turntablestepper.moveTo(reHomeMoveTo); // Set the position to move to
        reHomeMoveTo++; // increase by 1 for next move if needed
        turntablestepper.run();  // move 1 step 
      }
      // magnet is not detected
      // rotate CCW until the magnet is detected
      // since we are changing direction and want to go 1 step
      // at a time, need to set current position to 0
      turntablestepper.setCurrentPosition(0);  // also sets motor speed to 0
      turntablestepper.setMaxSpeed(speed);
      turntablestepper.setAcceleration(accel);
      reHomeMoveTo = -1;
      delay(500);
      while (!isMagnetDetected()) {  
        turntablestepper.moveTo(reHomeMoveTo);  // Set the position to move to
        reHomeMoveTo--;  // Decrease by 1 for next move if needed
        turntablestepper.run();  // move 1 step
      }
      // magnet has just hit the sensor when coming from 3:00 to 2:00
    }
  }  
  isTtblHomed = true;
  homeStopTurntable = 0;
  turntablestepper.setCurrentPosition(0);
  turntablestepper.setMaxSpeed(speed);
  turntablestepper.setAcceleration(accel);
  strcpy(ttblSelectedExit, "e");
  strcpy(ttblSelectedTrackEnd, "t");
  send_numbered_msg_to_tMsg(18);
}

void bFlipLoco_click() {
  isLocoFlipped = !isLocoFlipped;
  if (isShowLoco) {
    set_track_pictures_on_pgBasicsTurn();  // only refresh pictures if we are showing the loco
  }
}

void bAlign_click() {  
  // determine target position to move to;  call move_to_positionturn to do the move
  if (!isTtblHomed || isAlignGrayedOnBasicsTurn) {return;} 
  if (  // do nothing if already aligned
        ( 
          strcmp(currentLeftColor, "n") == 0 &&
          strcmp(currentSlantColor, "n") == 0
        )
          ||
        (
          strcmp(currentRightColor, "n") == 0 &&
          strcmp(currentStraightColor, "n") == 0
        )
    ) { return;}
    
  char fullVideoName[18]="e";  // examples:  e95.cww.h, e03.cww.t
  char baseVideoPlusDerivedStart[13];
  char derivedStartPoint[4];
  char synchedVideoEnd[4];
  char synchedVideoStart[4];
  char synchedVideoDirection[4];
  
  long targetPosition;
  if (strcmp(ttblSelectedExit, "e") == 0)  {
    strcpy(synchedVideoEnd,"3");
    strcat(fullVideoName, "03."); 
    if (strcmp(ttblSelectedTrackEnd, "h") == 0) 
    { targetPosition = unSavedSettings.turntableSettings.headEastStop; }
    else
    { targetPosition = unSavedSettings.turntableSettings.tailEastStop; }
  }
  else  // tblSelectedExit = "w"
  {
    strcpy(synchedVideoEnd,"95");
    strcat(fullVideoName, "95."); 
    if (strcmp(ttblSelectedTrackEnd, "h") == 0) 
    { targetPosition = unSavedSettings.turntableSettings.headWestStop; }
    else
    { targetPosition = unSavedSettings.turntableSettings.tailWestStop; }
  }

  if (isbTravelDirCW) {  //travel direction is CW
    strcat(fullVideoName, "cww."); 
    strcpy(synchedVideoDirection, "cw");
  }
  else {
    strcat(fullVideoName, "ccw."); 
    strcpy(synchedVideoDirection, "ccw");
  }

  if (isShowLoco) {
    if (isLocoFlipped) {
        if (strcmp(ttblSelectedTrackEnd,"t") == 0) {
          strcat(fullVideoName,"h");    
        }
        else {
          strcat(fullVideoName,"t");    
        }
    }
    else { strcat(fullVideoName,ttblSelectedTrackEnd); }
  }
  else { strcat(fullVideoName,"n"); }

  int i = 0;
  do {
    if (strcmp(videoNameToVideoID[i], fullVideoName) == 0) {
      videoIDTurntable = i;
      break;
    }
  i++;
  }
  while (i < 12);
  
  if (strcmp(ttblSelectedTrackEnd, "h") == 0) {
    if (strcmp(pointedSegmentName, "2") == 0) {
      strcpy(derivedStartPoint,"8");
    }
    else {
      if (strcmp(pointedSegmentName, "3") == 0) {
      strcpy(derivedStartPoint,"9");
      }
      else {
        if (strcmp(pointedSegmentName, "3.5") == 0) {
          strcpy(derivedStartPoint,"9.5");
        }
        else {
          if (strcmp(pointedSegmentName, "9") == 0) {
            strcpy(derivedStartPoint,"3");
          }
          else {  // must be "9.5"
            strcpy(derivedStartPoint,"3.5");
          }
        }
      }  
    }
  }
  else { 
    strcpy(derivedStartPoint, pointedSegmentName);
  }

  strncpy(baseVideoPlusDerivedStart, fullVideoName,7);
  baseVideoPlusDerivedStart[7] = '\0';
  strcpy(synchedVideoStart, derivedStartPoint);
  if (strcmp(synchedVideoStart,"3.5") == 0) {
    strcpy(synchedVideoStart, "35");
  }
  if (strcmp(synchedVideoStart,"9.5") == 0) {
    strcpy(synchedVideoStart, "95");
  }
  strcat(baseVideoPlusDerivedStart,derivedStartPoint);

  i=0;
  do {
    if (strcmp(videoNamePlusStartToTim[i].baseVideoNameAndDerivedStartPoint,
      baseVideoPlusDerivedStart) == 0) {
      currentTimValue = videoNamePlusStartToTim[i].timValue;
      break;  
    }
  i++;
  }
  while (i < 24);

  // calculate dis 
  long newVideoDis = 100;
  i=0;
  do {
    if (strcmp(unSavedSettings.turntableSettings.aTurntableSynchedVideo[i].start,
      synchedVideoStart) == 0 &&
      strcmp(unSavedSettings.turntableSettings.aTurntableSynchedVideo[i].end,
      synchedVideoEnd) == 0 &&
      strcmp(unSavedSettings.turntableSettings.aTurntableSynchedVideo[i].direction,
      synchedVideoDirection) == 0 &&
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[i].videoDis > (-1))
    {  
      newVideoDis = 
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[i].videoDis; 
      break;  
    }
  i++;
  }
  while (i < 24);

  // set vDisableStuff to disable other buttons
  Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand(); 
  // clear the input buffer so we don't process anything that's already stacked up
  while(Nextion.available()){Nextion.read();}    
  
  Nextion.print(F("vTurnTable.en=2")); NextionEndCommand();  // pause playing the video
  Nextion.print(F("vTurnTable.vid=")); Nextion.print(videoIDTurntable); // set videoID
  NextionEndCommand();  
  Nextion.print(F("vTurnTable.tim=")); Nextion.print(currentTimValue); // set start frame
  NextionEndCommand();  
  Nextion.print(F("vTurnTable.dis=")); // set video speed
  Nextion.print(newVideoDis);  NextionEndCommand();
  Nextion.print(F("vTurnTable.aph=127")); NextionEndCommand();  // show vTurnTable
  Nextion.print(F("vTurnTable.en=1")); NextionEndCommand();  // start playing the video
  
  isMotorTurnStopped = false;  // set state to "is running"
  isVideoTurnStopped = false;  // set state to "is running"
  move_to_positionTurn(targetPosition, isbTravelDirCW);
  isMotorTurnStopped = true;
  hideVideoIfItIsTimeTurn();
}

void bSlantedTrack_click() {
  if (isTtblHomed) {
    strcpy(ttblSelectedExit,"w");
  }
}

void bStraightTrack_click() {
  if (isTtblHomed) {
    strcpy(ttblSelectedExit,"e");
  }
}

void htLeft_click() {
  if (isTtblHomed) {
    if (strcmp(currentLeftColor,"g") == 0) {
      if (strcmp(ttblSelectedTrackEnd,"h") == 0) {
        strcpy(ttblSelectedTrackEnd,"t");
      }
      else {
        strcpy(ttblSelectedTrackEnd,"h");
      }
    }
  }
}

void htRight_click() {
  if (isTtblHomed) {
    if (strcmp(currentRightColor,"g") == 0) {
      if (strcmp(ttblSelectedTrackEnd,"h") == 0) {
        strcpy(ttblSelectedTrackEnd,"t");
      }
      else {
        strcpy(ttblSelectedTrackEnd,"h");
      }
    }
  }
}

void htBasAdjTurn_click() {
  Nextion.print(F("page pgAdjTurn")); NextionEndCommand();   
}

void htBasSettingsTurn_click() {
  Nextion.print(F("page pgSettings")); NextionEndCommand();   
}

void do_StartTurn_click() {
  unSavedSettings.turntableSettings = onDiskSettings.savedSettings.turntableSettings;
  strcpy(ttblSelectedTrackEnd,"");
  strcpy(ttblSelectedExit, "");
  isTtblHomed = false;

  // gray out the bMoveCW and bMoveCCW buttons
  Nextion.print(F("bMoveCW.pic=77")); NextionEndCommand();  
  Nextion.print(F("bMoveCW.pic2=77")); NextionEndCommand();  
  Nextion.print(F("bMoveCCW.pic=80")); NextionEndCommand();  
  Nextion.print(F("bMoveCCW.pic2=80")); NextionEndCommand();  
  isAlignGrayedOnBasicsTurn = true;
  // force " " on "Current Position"
  Nextion.print(F("tCurrentPos.txt=\" \"")); NextionEndCommand();   
  // set SlantedTrack icon to faded gray rails with label
  Nextion.print(F("bSlantedTrack.pic=30")); NextionEndCommand();   
    // set StraightTrack icon to faded gray rails with label
  Nextion.print(F("bStraightTrack.pic=39")); NextionEndCommand();   
    // set pMain pic to the faded gray rails and loco with labels
    // and yellow stuff to the same value
  Nextion.print(F("pMain.pic=15")); NextionEndCommand();   
  Nextion.print(F("vNormalPic.val=15")); NextionEndCommand();   
  Nextion.print(F("vYellowPicL.val=15")); NextionEndCommand();   
  Nextion.print(F("vYellowPicR.val=15")); NextionEndCommand();  

  
  // clear the input buffer as we can now safely accept more input
  while(Nextion.available()){Nextion.read();}
  
  reHomeTurn(true);  // force rehome to use CW
  if (isTtblHomed) {
    send_numbered_msg_to_tMsg(18);  // "Re-homing process complete"
  }
  // clear the input buffer as we can now safely accept more input
  while(Nextion.available()){Nextion.read();}
  // Serial.print("eeexit"); Serial.println(ttblSelectedExit);

}

void bMoveToHomeTurn_click() {
  if (isTtblHomed) {
    if (normalizeTurn(turntablestepper.currentPosition()) != 0) {
    move_to_positionTurn(0, isbSetDirCW);
  }
  else { send_numbered_msg_to_tMsg(70); } // turntable is already positioned 
  }
}

void bMoveToStopTurn_click() {
  long targetStop = -1;
  if (strcmp(lastStopName, "Startup Panel") == 0) {return;}
  if (strcmp(lastStopName, "Tail East") == 0) {
    targetStop = unSavedSettings.turntableSettings.tailEastStop;
  }
  if (strcmp(lastStopName, "Tail West") == 0) {
    targetStop = unSavedSettings.turntableSettings.tailWestStop;
  }
  if (strcmp(lastStopName, "Head East") == 0) {
    targetStop = unSavedSettings.turntableSettings.headEastStop;
  }
  if (strcmp(lastStopName, "Head West") == 0) {
    targetStop = unSavedSettings.turntableSettings.headWestStop;
  }
  if (targetStop != -1) {
    if (normalizeTurn(turntablestepper.currentPosition())  != targetStop) {
      move_to_positionTurn(targetStop, isbSetDirCW);
      // isSetDirCW says to honor the travel direction on pgAdjTurn
  }
  else { send_numbered_msg_to_tMsg(70); } // turntable is already positioned 
  }
}


void END_PGBASICSTURN_ROUTINES() {}

void START_PGADJTURN_ROUTINES() {}

void set_direction_of_travel_iconAdjTurn() {
  if (isbSetDirCW) { // show the CW icons on pgAdjTurn
    Nextion.print(F("bSetDir.pic=76")); NextionEndCommand();   
    Nextion.print(F("bSetDir.pic2=74")); NextionEndCommand();   
  }
  else {  // show the CCW icons
    Nextion.print(F("bSetDir.pic=188")); NextionEndCommand();   
    Nextion.print(F("bSetDir.pic2=187")); NextionEndCommand();   
  }
}


void set_data_on_pgAdjTurn() {
  char answer[15];
  if (!isTtblHomed) {
    Nextion.print(F("tCurrentPos.txt=\" \"")); NextionEndCommand();   
  }
  else {
    normalizespecial_turntable_currentpos_and_convert_to_text(answer);
    Nextion.print(F("tCurrentPos.txt=\"")); Nextion.print(answer);  
    Nextion.print(F("\"")); NextionEndCommand();   
  }
  set_direction_of_travel_iconAdjTurn();

  // set the stop values for the current stop.
  long current = -1;
  long saved = -1;
  long theDefault = -1;
  Nextion.print(F("tStopName.txt=\"")); Nextion.print(lastStopName);
  Nextion.print(F("\"")); NextionEndCommand();   
  
  if (strcmp(lastStopName,"Tail East") == 0) {
    current = unSavedSettings.turntableSettings.tailEastStop;
    saved = onDiskSettings.savedSettings.turntableSettings.tailEastStop;
    theDefault = defaultSettings.turntableSettings.tailEastStop;
    Nextion.print(F("tMoveName.txt=\"")); Nextion.print(lastStopName);  
    Nextion.print(F("\"")); NextionEndCommand();   
  }
  else {
    if (strcmp(lastStopName,"Tail West") == 0) {
      current = unSavedSettings.turntableSettings.tailWestStop;
      saved = onDiskSettings.savedSettings.turntableSettings.tailWestStop;
      theDefault = defaultSettings.turntableSettings.tailWestStop;
      Nextion.print(F("tMoveName.txt=\"")); Nextion.print(lastStopName);  
      Nextion.print(F("\"")); NextionEndCommand();   
    }
    else {
      if (strcmp(lastStopName,"Head East") == 0) {
        current = unSavedSettings.turntableSettings.headEastStop;
        saved = onDiskSettings.savedSettings.turntableSettings.headEastStop;
        theDefault = defaultSettings.turntableSettings.headEastStop;
        Nextion.print(F("tMoveName.txt=\"")); Nextion.print(lastStopName);  
        Nextion.print(F("\"")); NextionEndCommand();   
      }
      else {
        if (strcmp(lastStopName, "Head West") == 0) {
          current = unSavedSettings.turntableSettings.headWestStop;
          saved = onDiskSettings.savedSettings.turntableSettings.headWestStop;
          theDefault = defaultSettings.turntableSettings.headWestStop;
          Nextion.print(F("tMoveName.txt=\"")); Nextion.print(lastStopName);  
          Nextion.print(F("\"")); NextionEndCommand();   
        }
        else {
          if (strcmp(lastStopName, "Speed") == 0) {
            current = unSavedSettings.turntableSettings.speed;
            saved = onDiskSettings.savedSettings.turntableSettings.speed;
            theDefault = defaultSettings.turntableSettings.speed;
          }
          else {
            if (strcmp(lastStopName, "Acceleration") == 0) {
              current = unSavedSettings.turntableSettings.accel;
              saved = onDiskSettings.savedSettings.turntableSettings.accel;
              theDefault = defaultSettings.turntableSettings.accel;
            }
          }
        }
      }        
    }
  } 
  
  if (strcmp(lastStopName,"Startup Panel") == 0) {
    char aPanelName[12] = "";  
    Nextion.print(F("tCurrentValue.txt=\"")); 
    if (strcmp(unSavedSettings.startupPanel,"c") == 0) {
       strcpy(aPanelName,"Carriage"); }
    else { strcpy(aPanelName,"Turntable");}
    Nextion.print(aPanelName);
    Nextion.print(F("\"")); NextionEndCommand();   
    Nextion.print(F("tSavedValue.txt=\"")); 
    if (strcmp(onDiskSettings.savedSettings.startupPanel,"c") == 0) {
       strcpy(aPanelName,"Carriage"); }
    else { strcpy(aPanelName,"Turntable");}
    Nextion.print(aPanelName);
    Nextion.print(F("\"")); NextionEndCommand();   
    Nextion.print(F("tDefaultValue.txt=\"")); 
    if (strcmp(defaultSettings.startupPanel,"c") == 0) { 
      strcpy(aPanelName,"Carriage"); }
    else { strcpy(aPanelName,"Turntable");}
    Nextion.print(aPanelName);
    Nextion.print(F("\"")); NextionEndCommand();      

    Nextion.print(F("vis bSet,1")); NextionEndCommand();  
    Nextion.print(F("vis tSetExtra,1")); NextionEndCommand();  
    Nextion.print(F("vis bSpeedUp,0")); NextionEndCommand();  
    Nextion.print(F("vis bSpeedDown,0")); NextionEndCommand();  
    
    Nextion.print(F("vis tCustomLabel,0")); NextionEndCommand();  
    Nextion.print(F("vis tCustomFrame,0")); NextionEndCommand();  
    
    Nextion.print(F("vis tStepsLabel,0")); NextionEndCommand();  
    Nextion.print(F("vis tStepsFrame,0")); NextionEndCommand(); 
    Nextion.print(F("vis bStepDown,0")); NextionEndCommand();  
    Nextion.print(F("vis bStepUp,0")); NextionEndCommand();  
    Nextion.print(F("vis tSize,0")); NextionEndCommand();  
    
    Nextion.print(F("vis tActionLabel,0")); NextionEndCommand();  
    Nextion.print(F("vis tActionFrame,0")); NextionEndCommand(); 
    Nextion.print(F("vis bMoveCCW,0")); NextionEndCommand();  
    Nextion.print(F("vis bMoveCW,0")); NextionEndCommand();  
    
    Nextion.print(F("vis tStandardLabel,0")); NextionEndCommand(); 
    Nextion.print(F("vis tStandardFrame,0")); NextionEndCommand(); 
    Nextion.print(F("vis bMoveToStop,0")); NextionEndCommand(); 
    Nextion.print(F("vis bMoveToH,0")); NextionEndCommand(); 
    Nextion.print(F("vis bSetDir,0")); NextionEndCommand();  
    Nextion.print(F("vis tMoveName,0")); NextionEndCommand();  
    
    Nextion.print(F("vis bReHome,0")); NextionEndCommand(); 
  }
  else {
    if (strcmp(lastStopName, "Speed") == 0 ||
        strcmp(lastStopName, "Acceleration") == 0) {
      // display the current, default and 
      // saved values that were built earlier
      memset (answer, '\0', sizeof(answer));
      convert_steps_to_text(current, answer);
      Nextion.print(F("tCurrentValue.txt=\"")); 
      Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand();   
      convert_steps_to_text(saved, answer);
      Nextion.print(F("tSavedValue.txt=\"")); 
      Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand();   
      convert_steps_to_text(theDefault, answer);
      Nextion.print(F("tDefaultValue.txt=\"")); 
      Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand();  

      Nextion.print(F("vis bSet,0")); NextionEndCommand();  
      Nextion.print(F("vis tSetExtra,1")); NextionEndCommand();  
      Nextion.print(F("vis bSpeedUp,1")); NextionEndCommand();  
      Nextion.print(F("vis bSpeedDown,1")); NextionEndCommand();  
      
      Nextion.print(F("vis tCustomLabel,0")); NextionEndCommand();  
      Nextion.print(F("vis tCustomFrame,0")); NextionEndCommand();  
      
      Nextion.print(F("vis tStepsLabel,1")); NextionEndCommand();  
      Nextion.print(F("vis tStepsFrame,1")); NextionEndCommand(); 
      Nextion.print(F("vis bStepDown,1")); NextionEndCommand();  
      Nextion.print(F("vis bStepUp,1")); NextionEndCommand(); 
      Nextion.print(F("vis tSize,1")); NextionEndCommand();               
      
      Nextion.print(F("vis tActionLabel,0")); NextionEndCommand();  
      Nextion.print(F("vis tActionFrame,0")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveCCW,0")); NextionEndCommand();
      Nextion.print(F("vis bMoveCW,0")); NextionEndCommand();            
      
      Nextion.print(F("vis tStandardLabel,0")); NextionEndCommand(); 
      Nextion.print(F("vis tStandardFrame,0")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveToStop,0")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveToH,0")); NextionEndCommand(); 
      Nextion.print(F("vis bSetDir,0")); NextionEndCommand(); 
      Nextion.print(F("vis tMoveName,0")); NextionEndCommand();   
                  
      Nextion.print(F("vis bReHome,1")); NextionEndCommand(); 
    }   
    else {  
      // we are showing a stop
      // display the current, default 
      // and saved values that were built earlier
      memset (answer, '\0', sizeof(answer));
      convert_steps_to_text(current, answer);
      Nextion.print(F("tCurrentValue.txt=\"")); 
      Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand();   
      convert_steps_to_text(saved, answer);
      Nextion.print(F("tSavedValue.txt=\"")); 
      Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand();   
      convert_steps_to_text(theDefault, answer);
      Nextion.print(F("tDefaultValue.txt=\"")); 
      Nextion.print(answer);
      Nextion.print(F("\"")); NextionEndCommand();  

      Nextion.print(F("vis bSet,1")); NextionEndCommand();  
      Nextion.print(F("vis tSetExtra,1")); NextionEndCommand();  
      Nextion.print(F("vis bSpeedUp,0")); NextionEndCommand();  
      Nextion.print(F("vis bSpeedDown,0")); NextionEndCommand();  
      
      Nextion.print(F("vis tCustomLabel,1")); NextionEndCommand();  
      Nextion.print(F("vis tCustomFrame,1")); NextionEndCommand();  
      
      Nextion.print(F("vis tStepsLabel,1")); NextionEndCommand();  
      Nextion.print(F("vis tStepsFrame,1")); NextionEndCommand(); 
      Nextion.print(F("vis bStepDown,1")); NextionEndCommand();  
      Nextion.print(F("vis bStepUp,1")); NextionEndCommand();
      Nextion.print(F("vis tSize,1")); NextionEndCommand();                
      
      Nextion.print(F("vis tActionLabel,1")); NextionEndCommand();  
      Nextion.print(F("vis tActionFrame,1")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveCCW,1")); NextionEndCommand();
      Nextion.print(F("vis bMoveCW,1")); NextionEndCommand();    

      Nextion.print(F("vis tStandardLabel,1")); NextionEndCommand(); 
      Nextion.print(F("vis tStandardFrame,1")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveToStop,1")); NextionEndCommand(); 
      Nextion.print(F("vis bMoveToH,1")); NextionEndCommand(); 
      Nextion.print(F("vis bSetDir,1")); NextionEndCommand(); 
      Nextion.print(F("vis tMoveName,1")); NextionEndCommand();   
      
      Nextion.print(F("vis bReHome,1")); NextionEndCommand(); 
    }   
  }
 
  // put values on the panel
  Nextion.print(F("tSTrackEndVal.txt=\"")); 
  if (strcmp(ttblSelectedTrackEnd,"h") ==0) { Nextion.print(F("Head")); }
  else {Nextion.print(F("Tail"));}
  Nextion.print(F("\"")); NextionEndCommand();   

  Nextion.print(F("tSExitVal.txt=\"")); 
  if (strcmp(ttblSelectedExit,"e") ==0) { Nextion.print(F("East")); }
  else {Nextion.print(F("West"));}
  Nextion.print(F("\"")); NextionEndCommand();     
  Nextion.print(F("tSize.txt=\"")); 
  Nextion.print(unSavedSettings.turntableSettings.stepsize);
    Nextion.print(F("\"")); NextionEndCommand(); 
  
  // set tSetExtra
  Nextion.print(F("tSetExtra.txt=\"")); 
  if (strcmp(lastStopName,"Startup Panel") ==0) {
    if (strcmp(unSavedSettings.startupPanel, "c") == 0) {
      Nextion.print(F("         to\\rTURNTABLE"));
    }
    else {
      Nextion.print(F("        to\\rCARRIAGE"));
    }
  }
  else {
      if (strcmp(lastStopName,"Speed") == 0 ||
        strcmp(lastStopName,"Acceleration") == 0) {
        Nextion.print(unSavedSettings.turntableSettings.stepsize);    
      }
      else {
        Nextion.print(F("to the Current\\r    Position"));
      }
  }
  
  Nextion.print(F("\"")); NextionEndCommand(); 
  
  if (is_any_turntable_SynchVideo_needed()) { 
    Nextion.print(F("vis bSynchVideo,1")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchLabel,1")); NextionEndCommand();  
    Nextion.print(F("vis tSynchFrame,1")); NextionEndCommand();  
    Nextion.print(F("vis tSynchLabel2,1")); NextionEndCommand();  
    Nextion.print(F("vis tSynchCnt,1")); NextionEndCommand();  
    int cntOfVideosNeedingSynching = 0;
    for (int jj=0; jj<=23; jj++){
      if (is_TurntableSynchedVideo_A_Synch_Candidate(
        unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj], 
        unSavedSettings)) {
        cntOfVideosNeedingSynching = cntOfVideosNeedingSynching + 1;
      }
    }
    Nextion.print(F("tSynchCnt.txt=\""));
    Nextion.print(cntOfVideosNeedingSynching);
    Nextion.print(F("\"")); NextionEndCommand(); 
  }
  else { 
    Nextion.print(F("vis bSynchVideo,0")); NextionEndCommand(); 
    Nextion.print(F("vis tSynchLabel,0")); NextionEndCommand();  
    Nextion.print(F("vis tSynchFrame,0")); NextionEndCommand();  
    Nextion.print(F("vis tSynchLabel2,0")); NextionEndCommand();  
    Nextion.print(F("vis tSynchCnt,0")); NextionEndCommand();  
  }

  if (is_exclamation_needed()) {
    Nextion.print(F("vis pExclamation,1")); NextionEndCommand(); }
  else { Nextion.print(F("vis pExclamation,0")); NextionEndCommand();  }
}


void htAdjBasics_click() {
  long tempPos = normalizeTurn(turntablestepper.currentPosition());
  if (unSavedSettings.turntableSettings.tailEastStop == tempPos ||
      unSavedSettings.turntableSettings.tailWestStop == tempPos ||
      unSavedSettings.turntableSettings.headEastStop == tempPos ||
      unSavedSettings.turntableSettings.headWestStop == tempPos ||
      tempPos == homeStopTurntable) { // allow switching to another panel
        Nextion.print(F("page pgBasicsTurn")); NextionEndCommand();    
      }
  else {
        send_numbered_msg_to_tMsg(14); // move turntable to home or a stop first.
  }  
}

void htAdjSettings_click() {
  long tempPos = normalizeTurn(turntablestepper.currentPosition());
  if (unSavedSettings.turntableSettings.tailEastStop == tempPos ||
      unSavedSettings.turntableSettings.tailWestStop == tempPos ||
      unSavedSettings.turntableSettings.headEastStop == tempPos ||
      unSavedSettings.turntableSettings.headWestStop == tempPos ||
      tempPos == homeStopTurntable) { // allow switching to another panel
        Nextion.print(F("page pgSettings")); NextionEndCommand();    
      }
  else {
        send_numbered_msg_to_tMsg(14); // move turntable to home or a stop first.
  }  
}

void bSettingNext_click() {
  for (int ii=0; ii<=7; ii++){
    if (strcmp(lastStopName, ttblStopNameStepDown[ii]) == 0) {
      strcpy(lastStopName, ttblStopNameStepDown[ii+1]);
      break;
    }
  }
  set_data_on_pgAdjTurn();
}

void bSettingPrev_click() {
  for (int ii=7; ii>=0; ii--){
    if (strcmp(lastStopName, ttblStopNameStepDown[ii]) == 0) {
      strcpy(lastStopName, ttblStopNameStepDown[ii-1]);
      break;
    }
  }
  set_data_on_pgAdjTurn();
}

void bSpeedUpTurn_click() {
  long tmpSpeed = 0;
  if (strcmp(lastStopName, "Speed") == 0) {
    tmpSpeed = unSavedSettings.turntableSettings.speed;    
  }
  else {
      tmpSpeed = unSavedSettings.turntableSettings.accel;
  }
  tmpSpeed = tmpSpeed + unSavedSettings.turntableSettings.stepsize;
  if (strcmp(lastStopName, "Acceleration") == 0) {
    if (tmpSpeed > unSavedSettings.turntableSettings.speed) {  
      // msg:  accel can't be > speed
      send_numbered_msg_to_tMsg(0); 
      return;
    }
  }
  if (strcmp(lastStopName, "Speed") == 0) {
    unSavedSettings.turntableSettings.speed = tmpSpeed;
  }
  else {
    unSavedSettings.turntableSettings.accel = tmpSpeed;
  }
}    

void bSpeedDownTurn_click() {
  long tmpSpeed = 0;
  if (strcmp(lastStopName, "Speed") == 0) {
    tmpSpeed = unSavedSettings.turntableSettings.speed;    
  }
  else {
    tmpSpeed = unSavedSettings.turntableSettings.accel;
  }
  tmpSpeed = tmpSpeed - unSavedSettings.turntableSettings.stepsize;
  if (strcmp(lastStopName, "Acceleration") == 0) {
    if (tmpSpeed < 0 ) {  // msg:  accel can't be < 0
      send_numbered_msg_to_tMsg(1); 
      return;
    }
  }
  if (strcmp(lastStopName, "Speed") == 0) {
    if (tmpSpeed < 0 ) {  // msg:  speed can't be < 0
      send_numbered_msg_to_tMsg(2); 
      return;
    }
  }
  if (strcmp(lastStopName, "Acceleration") == 0) {
    if (tmpSpeed > unSavedSettings.turntableSettings.speed) {  // msg:  accel can't be > speed
      send_numbered_msg_to_tMsg(0); 
      return;
    }
  }
  if (strcmp(lastStopName, "Speed") == 0) {
    if (tmpSpeed < unSavedSettings.turntableSettings.accel) {  // msg:  accel can't be > speed
      send_numbered_msg_to_tMsg(0); 
      return;
    }
  }
  if (strcmp(lastStopName, "Speed") == 0) {
    unSavedSettings.turntableSettings.speed = tmpSpeed;
  }
  else {
    unSavedSettings.turntableSettings.accel = tmpSpeed;
  }
}

bool is_TurntableSynchedVideo_A_Synch_Candidate(
  SynchedVideoTurntable &inputTurntableSynchedVideo,
  SettingsForOneBatch &aBatch) {
  long newStepCount = 
    calculate_TurntableVideo_stepCount(inputTurntableSynchedVideo);
  if (aBatch.turntableSettings.speed !=
      inputTurntableSynchedVideo.speed ||
      aBatch.turntableSettings.accel !=
      inputTurntableSynchedVideo.accel ||
      newStepCount != inputTurntableSynchedVideo.stepCount
    ) { return true;}
  else { return false; }
}

void bSynchVideoTurn_click() {
  int cntOfVideosNeedingModification = 0;
  int cntOfCurrentVideoBeingModified = 0;
  char ofMsg[30];
  char myBuffer[4];
  long newStepCount = 0;
  float tmp1 = 0;

  for (int jj=0; jj<=23; jj++){
    if (is_TurntableSynchedVideo_A_Synch_Candidate(
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj],
      unSavedSettings)) {
      cntOfVideosNeedingModification = cntOfVideosNeedingModification + 1;
    }
  }
  if (cntOfVideosNeedingModification == 0) {return;}
  strcpy(ofMsg," of ");
  itoa(cntOfVideosNeedingModification, myBuffer,10);
  strcat(ofMsg, myBuffer);
  strcat(ofMsg, " video(s) being synched...");

  // disable the turntable motor so we don't physically move the turntable
  // during synching
  digitalWrite(turntableEnbl,HIGH);  
  
  for (int ii=0; ii<=23; ii++){
    if (is_TurntableSynchedVideo_A_Synch_Candidate(
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii],
      unSavedSettings)) {
      
      Nextion.print(F("tMsg.txt=\"")); 
      strcpy(myBuffer,"");
      cntOfCurrentVideoBeingModified = cntOfCurrentVideoBeingModified + 1;
      Nextion.print(cntOfCurrentVideoBeingModified);
      Nextion.print(ofMsg);
      Nextion.print(F("\"")); NextionEndCommand(); 
      
        // run motor:  use newStepCount, speed, accel
        // capture elapsed time in ms
        // calculate videoDis
        // in aTurntableSynchedVideo[ii], store:
        // stepCount, speed, accel, videoDis
      newStepCount = calculate_TurntableVideo_stepCount(
        unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii]);  
        
      if (newStepCount != 0) {
        turntablestepper.setCurrentPosition(0);
        turntablestepper.setMaxSpeed(unSavedSettings.turntableSettings.speed);
        turntablestepper.setAcceleration(unSavedSettings.turntableSettings.accel);
        elapsedMotorRun = 0;
        startAlignMS = millis();
        move_to_positionTurn(newStepCount, true);
        elapsedMotorRun = millis() - startAlignMS;
        delay(300);
        tmp1 = (float)aTurntableBaseVideo[ii].baseVideoMS / (float)elapsedMotorRun;
        unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].videoDis = tmp1 * 100;
      }
      else {
        unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].videoDis = 100;
      }
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].stepCount = newStepCount;  
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].speed = 
        unSavedSettings.turntableSettings.speed;
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[ii].accel = 
        unSavedSettings.turntableSettings.accel;
      set_data_on_pgAdjTurn();
    }
  }
  set_data_on_pgAdjTurn();
  // enable the turntable motor so we can re-home
  digitalWrite(turntableEnbl,LOW);  
  // when you enable the motor, you can't predict where the motor winds up.
  //  it might jump when it is energized
  delay(500);  // give the motor a bit to settle down
  send_numbered_msg_to_tMsg(61);  // Stepper is homing...
  reHomeTurn(true);  // force re-home to use CW 
  set_data_on_pgAdjTurn();
}

void bSetStop_click() {
  long tempPos = normalizeTurn(turntablestepper.currentPosition());
  if (strcmp(lastStopName,"Tail East") == 0) {
    if (unSavedSettings.turntableSettings.tailEastStop != tempPos) {
      if (0 < tempPos && tempPos < unSavedSettings.turntableSettings.headWestStop) {
        // all is good
        unSavedSettings.turntableSettings.tailEastStop = tempPos;
        send_numbered_msg_to_tMsg(103); 
      }
      else {
        send_numbered_msg_to_tMsg(45); 
      }
    }
    return;  
  }
  if (strcmp(lastStopName,"Tail West") == 0) {
    if (unSavedSettings.turntableSettings.tailWestStop != tempPos) {
      if (unSavedSettings.turntableSettings.headEastStop < tempPos &&
        tempPos < stepsPerTurntableRevolution) {
          // all is good
          unSavedSettings.turntableSettings.tailWestStop = tempPos;
          send_numbered_msg_to_tMsg(103); 
        }
      else {  
        send_numbered_msg_to_tMsg(45); 
      }
    }
    return;  
  }

  if (strcmp(lastStopName,"Head East") == 0) {
    if (unSavedSettings.turntableSettings.headEastStop != tempPos) {
      if (unSavedSettings.turntableSettings.headWestStop < tempPos &&
        tempPos < unSavedSettings.turntableSettings.tailWestStop) {
          // all is good
          unSavedSettings.turntableSettings.headEastStop =  tempPos;
          send_numbered_msg_to_tMsg(103); 
        }
      else {
        send_numbered_msg_to_tMsg(45); 
      }
    }
    return;  
  }

  if (strcmp(lastStopName, "Head West") == 0) {
    if (unSavedSettings.turntableSettings.headWestStop != tempPos) {
      if (unSavedSettings.turntableSettings.tailEastStop < tempPos &&
        tempPos < unSavedSettings.turntableSettings.headEastStop) {
          // all is good
          unSavedSettings.turntableSettings.headWestStop = tempPos;
          send_numbered_msg_to_tMsg(103); 
        }
      else {
        send_numbered_msg_to_tMsg(45); 

      }
    }
    return;  
  }

  if (strcmp(lastStopName, "Startup Panel") == 0) {
    if (strcmp(unSavedSettings.startupPanel, "c") == 0) {
    strcpy(unSavedSettings.startupPanel, "t");
    }
    else {
      strcpy(unSavedSettings.startupPanel, "c");
    }
    send_numbered_msg_to_tMsg(103); 
    return;
  }
}


void cycleStepSizeTurn(int direction) {
  // direction is positive (or zero) for "make bigger";  negative for "make smaller"
  int trueDirection = 1;
  if (direction < 0) { trueDirection = -1; }
  // find current step size in the allSteps array
  int found = -1;
  for (int ptr = 0; ptr < 21; ptr++) {
    if (unSavedSettings.turntableSettings.stepsize == allSteps[ptr]) {
      found = ptr;
      break;
    }
  }
  if (found == -1) {found = 3;}  // point "found" to a reasonable default
  if (trueDirection == 1) {
    found--;  // we move "down" the array to "move bigger"
    if (found < 0) { found = 7;}
  }
  else {
    found++;  // we move "up" the array to "make smaller"
    if (found > 7) { found = 0; }
  }
  unSavedSettings.turntableSettings.stepsize = allSteps[found];
  set_data_on_pgAdjTurn();
}

void bMoveCCW_click() {
  if (isTtblHomed) {
    long stepCount = unSavedSettings.turntableSettings.stepsize;
    stepCount = -1 * stepCount;
    move_turntable(stepCount);
  }  
}

void bMoveCW_click() {
  if (isTtblHomed) {
    move_turntable(unSavedSettings.turntableSettings.stepsize);
  }  
}

void END_PGADJTURN_ROUTINES() {}

void START_PGSETTINGS_ROUTINES() {}

void build_pgSettings_Turntable_Stuff() {
  char answer[15];
  bool isBlackNeeded = false;
// tail east
  Nextion.print(F("tTtblTailEast.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.tailEastStop, answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.tailEastStop !=
    onDiskSettings.savedSettings.turntableSettings.tailEastStop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }
  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.tailEastStop,
    answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.tailEastStop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblTailEast.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblTailEast.pco=65535"); NextionEndCommand();
  }
// tail west
  isBlackNeeded =false; 
  Nextion.print(F("tTtblTailWest.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.tailWestStop, answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.tailWestStop !=
    onDiskSettings.savedSettings.turntableSettings.tailWestStop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.tailWestStop, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.tailWestStop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblTailWest.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblTailWest.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false; 
  Nextion.print(F("tTtblHeadEast.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.headEastStop, answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.headEastStop !=
    onDiskSettings.savedSettings.turntableSettings.headEastStop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.headEastStop, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.headEastStop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblHeadEast.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblHeadEast.pco=65535"); NextionEndCommand();
  }

  
  isBlackNeeded = false;
  Nextion.print(F("tTtblHeadWest.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.headWestStop, answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.headWestStop !=
    onDiskSettings.savedSettings.turntableSettings.headWestStop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.headWestStop, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.headWestStop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblHeadWest.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblHeadWest.pco=65535"); NextionEndCommand();
  }

  
  isBlackNeeded = false;
  Nextion.print(F("tTtblStepSize.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.stepsize, answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.stepsize !=
    onDiskSettings.savedSettings.turntableSettings.stepsize) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.stepsize, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.stepsize, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblStepSize.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblStepSize.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false;
  Nextion.print(F("tTtblNormSpeed.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.speed, 
    answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.speed !=
    onDiskSettings.savedSettings.turntableSettings.speed) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.speed,
    answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.speed,
    answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblNormSpeed.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblNormSpeed.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false;
  Nextion.print(F("tTtblNormAccel.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.turntableSettings.accel, 
    answer);
  Nextion.print(answer);
  if (unSavedSettings.turntableSettings.accel !=
    onDiskSettings.savedSettings.turntableSettings.accel) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.turntableSettings.accel,
    answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.turntableSettings.accel,
    answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblNormAccel.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblNormAccel.pco=65535"); NextionEndCommand();
  }

   
  isBlackNeeded = false;
  Nextion.print(F("tTtblVidDiff.txt=\""));  
  int cntOfVideosNeedingSaving = 0;
  for (int jj=0; jj<=23; jj++){
    if ((unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj].accel !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[jj].accel)
      || (unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj].speed !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[jj].speed)
      || (unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj].stepCount !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[jj].stepCount)
      || (unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj].videoDis !=
      onDiskSettings.savedSettings.turntableSettings.aTurntableSynchedVideo[jj].videoDis))
    {
      cntOfVideosNeedingSaving = cntOfVideosNeedingSaving + 1;
    }
  }
  
  if (cntOfVideosNeedingSaving == 0) {
    Nextion.print(F("0"));  
  }
  else {
    Nextion.print(cntOfVideosNeedingSaving);
    Nextion.print(F(" (*)")); 
    isBlackNeeded = true; 
  }
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblVidDiff.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblVidDiff.pco=65535"); NextionEndCommand();
  }


  isBlackNeeded = false;
  Nextion.print(F("tTtblVidSynch.txt=\""));  
  int cntOfVideosNeedingSynching = 0;
  for (int jj=0; jj<=23; jj++){
    if (is_TurntableSynchedVideo_A_Synch_Candidate(
      unSavedSettings.turntableSettings.aTurntableSynchedVideo[jj], 
      unSavedSettings)) {
      cntOfVideosNeedingSynching = cntOfVideosNeedingSynching + 1;
    }
  }
  Nextion.print(cntOfVideosNeedingSynching);  
  if (cntOfVideosNeedingSynching > 0) {
    Nextion.print(F(" (**)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tTtblVidSynch.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tTtblVidSynch.pco=65535"); NextionEndCommand();
  }
}


void build_pgSettings_Carriage_Stuff() {

  char answer[15];
  bool isBlackNeeded = false;

  Nextion.print(F("tCarrTrack1.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.carriageSettings.track1stop, answer);
  Nextion.print(answer);
  if (unSavedSettings.carriageSettings.track1stop !=
    onDiskSettings.savedSettings.carriageSettings.track1stop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.carriageSettings.track1stop, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.carriageSettings.track1stop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrTrack1.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrTrack1.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false;
  Nextion.print(F("tCarrTrack2.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.carriageSettings.track2stop, answer);
  Nextion.print(answer);
  if (unSavedSettings.carriageSettings.track2stop !=
    onDiskSettings.savedSettings.carriageSettings.track2stop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.carriageSettings.track2stop, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.carriageSettings.track2stop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrTrack2.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrTrack2.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false;
  Nextion.print(F("tCarrTrack3.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.carriageSettings.track3stop, answer);
  Nextion.print(answer);
  if (unSavedSettings.carriageSettings.track3stop !=
    onDiskSettings.savedSettings.carriageSettings.track3stop) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.carriageSettings.track3stop, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.carriageSettings.track3stop, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrTrack3.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrTrack3.pco=65535"); NextionEndCommand();
  }  

  isBlackNeeded = false;
  Nextion.print(F("tCarrStepSize.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.carriageSettings.stepsize, answer);
  Nextion.print(answer);
  if (unSavedSettings.carriageSettings.stepsize !=
    onDiskSettings.savedSettings.carriageSettings.stepsize) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.carriageSettings.stepsize, answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.carriageSettings.stepsize, answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrStepSize.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrStepSize.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false;
  Nextion.print(F("tCarrNormSpeed.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.carriageSettings.speed, 
    answer);
  Nextion.print(answer);
  if (unSavedSettings.carriageSettings.speed !=
    onDiskSettings.savedSettings.carriageSettings.speed) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.carriageSettings.speed,
    answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.carriageSettings.speed,
    answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrNormSpeed.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrNormSpeed.pco=65535"); NextionEndCommand();
  }  

  isBlackNeeded = false;
  Nextion.print(F("tCarrNormAccel.txt=\""));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(unSavedSettings.carriageSettings.accel, 
    answer);
  Nextion.print(answer);
  if (unSavedSettings.carriageSettings.accel !=
    onDiskSettings.savedSettings.carriageSettings.accel) {
    Nextion.print(F(" (*)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(onDiskSettings.savedSettings.carriageSettings.accel,
    answer);
  Nextion.print(answer);

  Nextion.print(F("\\r"));  
  memset (answer, '\0', sizeof(answer));
  convert_steps_to_text(defaultSettings.carriageSettings.accel,
    answer);
  Nextion.print(answer);
    
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrNormAccel.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrNormAccel.pco=65535"); NextionEndCommand();
  }  

    
  isBlackNeeded = false;
  Nextion.print(F("tStartup.txt=\""));  
  if (strcmp(unSavedSettings.startupPanel, "c") ==0) { Nextion.print(F("Carriage")); }
  else { Nextion.print(F("Turntable")); }
  if (strcmp(unSavedSettings.startupPanel,
    onDiskSettings.savedSettings.startupPanel) != 0) {
      Nextion.print(F(" (*)"));
      isBlackNeeded = true;
  }

  Nextion.print(F("\\r"));  
  if (strcmp(onDiskSettings.savedSettings.startupPanel, "c") ==0) { Nextion.print(F("Carriage")); }
  else { Nextion.print(F("Turntable")); }
   
  Nextion.print(F("\\r"));  
  if (strcmp(defaultSettings.startupPanel, "c") ==0) { Nextion.print(F("Carriage")); }
  else { Nextion.print(F("Turntable")); }

  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tStartup.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tStartup.pco=65535"); NextionEndCommand();
  }  

  isBlackNeeded = false;
  Nextion.print(F("tCarrVidDiff.txt=\""));  
  int cntOfVideosNeedingSaving = 0;
  for (int jj=0; jj<=8; jj++){
    if ((          unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj].accel !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[jj].accel)

      || (         unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj].speed !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[jj].speed)

      || (         unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj].stepCount !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[jj].stepCount)

      || (         unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj].videoDis !=
      onDiskSettings.savedSettings.carriageSettings.aCarriageSynchedVideo[jj].videoDis))
    {
      cntOfVideosNeedingSaving = cntOfVideosNeedingSaving + 1;
    }
  }
  
  if (cntOfVideosNeedingSaving == 0) {
    Nextion.print(F("0"));  
  }
  else {
    Nextion.print(cntOfVideosNeedingSaving);
    Nextion.print(F(" (*)")); 
    isBlackNeeded = true; 
  }
  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrVidDiff.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrVidDiff.pco=65535"); NextionEndCommand();
  }

  isBlackNeeded = false;
  Nextion.print(F("tCarrVidSynch.txt=\""));  
  int cntOfVideosNeedingSynching = 0;
  for (int jj=0; jj<=8; jj++){
    if (is_CarriageSynchedVideo_A_Synch_Candidate(unSavedSettings.carriageSettings.aCarriageSynchedVideo[jj], 
      unSavedSettings)) {
      cntOfVideosNeedingSynching = cntOfVideosNeedingSynching + 1;
    }
  }
  Nextion.print(cntOfVideosNeedingSynching);  
  if (cntOfVideosNeedingSynching > 0) {
    Nextion.print(F(" (**)"));
    isBlackNeeded = true;
  }

  Nextion.print(F("\"")); NextionEndCommand();
  if (isBlackNeeded) {
    Nextion.print("tCarrVidSynch.pco=0"); NextionEndCommand();
  }
  else {
    Nextion.print("tCarrVidSynch.pco=65535"); NextionEndCommand();
  }  
}

void set_data_on_pgSettings() {
  if (strcmp(saveSettingsMotor,"C") ==0) {
    Nextion.print(F("tBasicsLabel.txt=\"Carriage Basics\"")); NextionEndCommand();
    Nextion.print(F("tSettingsLabel.txt=\"Carriage Settings\"")); NextionEndCommand();
  }
  else {
    Nextion.print(F("tBasicsLabel.txt=\"Turntable Basics\"")); NextionEndCommand();
    Nextion.print(F("tSettingsLabel.txt=\"Turntable Settings\"")); NextionEndCommand();
  }
  build_pgSettings_Turntable_Stuff();
  build_pgSettings_Carriage_Stuff();
  if (is_exclamation_needed()) {
    Nextion.print(F("vis pExclamation,1")); NextionEndCommand(); }
  else { Nextion.print(F("vis pExclamation,0")); NextionEndCommand();  }
}

void bCarrSaved_click() {
  unSavedSettings.carriageSettings = onDiskSettings.savedSettings.carriageSettings;
  strcpy(unSavedSettings.startupPanel, onDiskSettings.savedSettings.startupPanel);
  send_numbered_msg_to_tMsg(25); // "carriage reset from saved
  set_data_on_pgSettings();
}

void bCarrDefault_click() {
  unSavedSettings.carriageSettings = defaultSettings.carriageSettings;
  strcpy(unSavedSettings.startupPanel, defaultSettings.startupPanel);
  send_numbered_msg_to_tMsg(27); // "carriage reset from default  
  set_data_on_pgSettings();
}

void bTtblSaved_click() {
  unSavedSettings.turntableSettings = onDiskSettings.savedSettings.turntableSettings;
  strcpy(unSavedSettings.startupPanel, onDiskSettings.savedSettings.startupPanel);
  send_numbered_msg_to_tMsg(26); // "turntable reset from saved  
  set_data_on_pgSettings();  
}

void bTtblDefault_click() {
  unSavedSettings.turntableSettings = defaultSettings.turntableSettings;
  strcpy(unSavedSettings.startupPanel, defaultSettings.startupPanel);
  send_numbered_msg_to_tMsg(28); // "turntable reset from default  
  set_data_on_pgSettings();  
}

void bSave_click() {
  if (ispgBasicsStarted && homeStopCarriage != 0) {
    // " cannot save because homestop is non-zero
    send_numbered_msg_to_tMsg(46); 
    return;
  }
  if (ispgBasicsTurnStarted && !isTurntableStopsOK()) {
    // "tracks stops must be in increasing order" message
    send_numbered_msg_to_tMsg(45); 
    return;  
  }
  if ((unSavedSettings.carriageSettings.stepsize <= 0) 
    || (unSavedSettings.turntableSettings.stepsize <= 0)) {
      // "all step sizes must be > 0
      send_numbered_msg_to_tMsg(34); 
    return;  
  }
  if (
    (unSavedSettings.carriageSettings.speed <=0)
    || (unSavedSettings.turntableSettings.speed <=0)) {
    send_numbered_msg_to_tMsg(33); // "all speeds must be > 0
    return;  
  }
  if (
    (unSavedSettings.carriageSettings.accel <=0)
    || (unSavedSettings.turntableSettings.accel <=0)) {
    send_numbered_msg_to_tMsg(32); // "all accels must be > 0
    return;  
  }
  if (
    (unSavedSettings.carriageSettings.speed <= 
      unSavedSettings.carriageSettings.accel)
    || (unSavedSettings.turntableSettings.speed <= 
      unSavedSettings.turntableSettings.accel)) {
    send_numbered_msg_to_tMsg(31); // "all speeds must be > accel
    return;  
  }

  if ((0 < unSavedSettings.turntableSettings.tailEastStop) 
    && (unSavedSettings.turntableSettings.tailEastStop <
      unSavedSettings.turntableSettings.headWestStop) 
    && (unSavedSettings.turntableSettings.headWestStop <
      unSavedSettings.turntableSettings.headEastStop) 
    && (unSavedSettings.turntableSettings.headEastStop <
      unSavedSettings.turntableSettings.tailWestStop)) {
      // all is good
    }
  else {
    send_numbered_msg_to_tMsg(45); 
    // "all turntable stops must be increasingl
    return;    
  }
  onDiskSettings.savedSettings = unSavedSettings;
  EEPROM.put(eeAddress, onDiskSettings);
  send_numbered_msg_to_tMsg(47); // "items saved" msg
}

void htSetBasics_click() {
  if (strcmp(saveSettingsMotor,"C") ==0) {
    if (unSavedSettings.carriageSettings.track1stop == mystepper.currentPosition() ||
        unSavedSettings.carriageSettings.track2stop == mystepper.currentPosition() ||
        unSavedSettings.carriageSettings.track3stop == mystepper.currentPosition() ||
        mystepper.currentPosition() == 0) { // allow switching to carriage basics
          Nextion.print(F("page pgBasics")); NextionEndCommand();    
      }
      else {
        send_numbered_msg_to_tMsg(8); 
        // move carriage to home or a stop from Settings first
    }
  }
  else {
    long tempPos = normalizeTurn(turntablestepper.currentPosition());
    if (unSavedSettings.turntableSettings.tailEastStop == tempPos ||
        unSavedSettings.turntableSettings.tailWestStop == tempPos ||
        unSavedSettings.turntableSettings.headEastStop == tempPos ||
        unSavedSettings.turntableSettings.headWestStop == tempPos ||
        tempPos == homeStopTurntable) { // allow switching to turntable basics
          Nextion.print(F("page pgBasicsTurn")); NextionEndCommand();    
       }
    else {
      send_numbered_msg_to_tMsg(9); 
      // move turntable to home or a stop from Settings first
    }  
  }
}

void htSetAdj_click() {
if (strcmp(saveSettingsMotor,"C") ==0) {
    Nextion.print(F("page pgAdjustments")); NextionEndCommand();
  }
  else {
    Nextion.print(F("page pgAdjTurn")); NextionEndCommand();
  }
}

void END_PGSETTINGS_ROUTINES() {}

void START_HELP_PAGES_ROUTINES() {}

void set_data_on_pgHSetting0() {
  if (strcmp(saveSettingsMotor,"C") ==0) {
    Nextion.print(F("tBasicsLabel.txt=\"Carriage Basics\"")); NextionEndCommand();
    Nextion.print(F("tSettingsLabel.txt=\"Carriage Settings\"")); NextionEndCommand();
  }
  else {
    Nextion.print(F("tBasicsLabel.txt=\"Turntable Basics\"")); NextionEndCommand();
    Nextion.print(F("tSettingsLabel.txt=\"Turntable Settings\"")); NextionEndCommand();
  }
}

void END_HELP_PAGES_ROUTINES() {}

void START_NEXTION_COMMON_ROUTINES() {}


void page_refresh(uint8_t page) {
    // page identifies the Nextion panel being refreshed.  
    switch(page){
        case 0x01: // pgBasics
        {
          strcpy(saveSettingsMotor,"C");
          set_data_on_pgBasics();
        }
        break;
        case 0x02: // pgAdjustments
        {  
          set_data_on_pgAdjustments();  
        }  
        break;
        case 0x03: // pgSettings
        {
          set_data_on_pgSettings();
        }  
        break;
        case 0x04: // pgBasicsTurn
        {
          strcpy(saveSettingsMotor,"T");
          set_data_on_pgBasicsTurn(true);
        }
        break;
        case 0x05: // pgAdjTurn
        {
          set_data_on_pgAdjTurn();
        }
        break;
         case 0x06: // pgHSetting0
        {
          set_data_on_pgHSetting0();
        }
        break;
    }
}    

void button_clicked(uint8_t buttonid) {
  switch(buttonid){
      /*       pgBasics
      0        bTrack3
      1        bTrack2
      2        bTrack1
      4        bStart
      17       htBasAdj
      18       htBasSettings
      19       bHelp
      1B       bAlign
      */
    case 0x00: // bTrack3
    {  
      if (checkIsPgBasicsStartedTrue()) {
          carrSelectedExit = 3;
          set_data_on_pgBasics();
      }  
    }  
    break;
    case 0x01: // bTrack2
    { 
      if (checkIsPgBasicsStartedTrue()) {
          carrSelectedExit = 2;
          set_data_on_pgBasics();
      }  
    } 
    break;
    case 0x02: // bTrack1
    { 
      if (checkIsPgBasicsStartedTrue()) {
          carrSelectedExit = 1;
          set_data_on_pgBasics();
      }  
    }  
    break;
    case 0x04: // bStart  
    { 
      if (ispgBasicsStarted == false) {
        send_numbered_msg_to_tMsg(6);
        ispgBasicsStarted = true;
        Nextion.print(F("vis bStart,0")); NextionEndCommand();  // hide Start
        Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();     // disable other buttons  
        do_initializationCarriage();
        Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();     // enable other buttons
      }
    }  
    break;   
    case 0x17: // htBasAdj
    { htBasAdj_Click(); }
    break;
    case 0x18: // htBasSettings
    { htBasSettings_Click(); }
    break;
    case 0x19: // bHelp
    { 
      if (checkIsPgBasicsStartedTrue()) {
          Nextion.print(F("page pgHBasic0")); NextionEndCommand();  // goto Help
      }  
    }
    break;
    case 0x1B: // bAlign
    { bAlignCarr_Click(); }
    break;
       
    /*    pgAdjustments
      5        bSettingNext
      6        bSet
      7        bMoveDown
      8        bMoveToStop
      0B       bStepDown
      0C       bMoveToH
      0D       bMoveUp
      0F       bStepUp 
      10       bSynchVideo
      11       bReHome
      12       htAdjBasics
      13       htAdjSettings

      1A       bSettingPrev
      1C       bSpeedUp
      1D       bSpeedDown
      
    */
    case 0x05: // bSettingNext
    {
      bSettingNextCarriage_click();
      set_data_on_pgAdjustments();
    }  
    break;
    case 0x06: // bSet
    {
      bSetStopCarriage_click();
      set_data_on_pgAdjustments();
    }
    break;
    case 0x07: // bMoveDown
    { 
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons
      bMoveDown_click();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons  
    }    
    break;
    case 0x08: // bMoveToStop
    { 
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons
      bMoveToStop_click(); 
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons  
    }    
    break;
    case 0x0B: // bStepPrev
    {
      cycleStepSize(-1);
      set_data_on_pgAdjustments();
    }
    break;
    case 0x0C: // bMoveToH
    {   
      if (mystepper.currentPosition() == homeStopCarriage) {break;}
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons      
      move_to_stop((int) 0,false);
      set_data_on_pgAdjustments();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons
    }    
    break;
    case 0x0D: // bMoveUp
    {
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons
      bMoveUp_click();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons  
    }    
    break;
    case 0x0F: // bStepNext
    {
      cycleStepSize(1);
      set_data_on_pgAdjustments();
    }
    break;
    case 0x10:  // bSynchVideo
    {
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons      
      bSynchVideoCarr_click();
      set_data_on_pgAdjustments();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons 
    }
    break;
    case 0x11: // bReHome 
    { 
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  // set vDisableStuff to disable other buttons  
      reHome();
      set_data_on_pgAdjustments();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  // set vDisableStuff to enable other buttons  
    }    
    break;
    case 0x12:  // htAdjBasics
    { htAdjBasicsPgAdjustments_click(); }
    break;
    case 0x13:  // htAdjSettings
    { htAdjSettingsPgAdjustments_click(); }
    break;
    case 0x1A: // bSettingPrev
    {
      bSettingPrevCarriage_click();
      set_data_on_pgAdjustments();
    }  
    break;
    case 0x1C:  // bSpeedUp
    {
      bSpeedUpCarriage_click();
      set_data_on_pgAdjustments();
    }
    break;

    case 0x1D:  // bSpeedDown
    {
        bSpeedDownCarriage_click();
        set_data_on_pgAdjustments();
    }
    break;
   
    /*          pgSettings
      14       bSave
      60       htSetBasics
      61       htSetAdj
      70       bCarrSaved
      71       bCarrDefault
      72       bTtblSaved
      73       bTtblDefault
    */
    case 0x14: // bSave
    { 
      bSave_click();
      set_data_on_pgSettings();
    }    
    break;
    case 0x60:
    {
      htSetBasics_click();
    }
    break;
    case 0x61:
    {
      htSetAdj_click();
    }
    break;
    case 0x70:
    {
      bCarrSaved_click();
    }
    break;
     case 0x71:
    {
      bCarrDefault_click();
    }
    break;
    case 0x72:
    {
      bTtblSaved_click();
    }
    break;
    case 0x73:
    {
      bTtblDefault_click();
    }
    break;
    /*    pgBasicsTurn
      30  bMoveCW 
      31  bSlantedTrack 
      32  htLeft 
      33  bStart 
      34  bShowLoco
      35  bMoveCCW
      36  htRight
      37  bStraightTrack 
      38  bFlipLoco
      39  htBasAdj
      3A  htBasSettings
      3B  bHelp
    
    */
    case 0x30:  // bMoveCW
    { if (checkIsPgBasicsTurnStartedTrue()) {
        isbTravelDirCW = true;
        bAlign_click();
      }
    }
    break;
    case 0x31:  // bSlantedTrack
    { 
      if (checkIsPgBasicsTurnStartedTrue()) {
      bSlantedTrack_click(); 
      set_data_on_pgBasicsTurn(true);}
    }
    break;
    case 0x32:  // htLeft
    { if (checkIsPgBasicsTurnStartedTrue()) {
      htLeft_click();
      set_data_on_pgBasicsTurn();
      }
    }
    break;
    case 0x33:  // bStart
    {
      if (ispgBasicsTurnStarted == false) {
        send_numbered_msg_to_tMsg(6);
        ispgBasicsTurnStarted = true;
        Nextion.print(F("vis bStart,0")); NextionEndCommand();  // hide Start
        Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand(); // disable other buttons  
        do_StartTurn_click(); 
        set_data_on_pgBasicsTurn(true);
        Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand(); // enable other buttons
      }
    }
    break;
    case 0x34:  // bShowLoco
    {if (checkIsPgBasicsTurnStartedTrue()) {
      bShowLoco_click();}
    }
    break;
    case 0x35:  // bMoveCCW
    { 
      if (checkIsPgBasicsTurnStartedTrue()) {
        isbTravelDirCW = false;
        bAlign_click();
      }  
    }
    break;
    case 0x36:  // htRight
    { if (checkIsPgBasicsTurnStartedTrue()) {
      htRight_click();
      set_data_on_pgBasicsTurn();
      }
    }
    break;
    case 0x37:  // bStraightTrack
    { if (checkIsPgBasicsTurnStartedTrue()) {
      bStraightTrack_click();
      set_data_on_pgBasicsTurn();}
      }
    break;
    case 0x38:  // bFlipLoco
    { if (checkIsPgBasicsTurnStartedTrue()) {
      bFlipLoco_click();}
    }
    break;
    case 0x39:  // htBasAdjTurn
    { if (checkIsPgBasicsTurnStartedTrue()) {
      htBasAdjTurn_click();}
    }
    break;
    case 0x3A:  // htBasSettings
    { if (checkIsPgBasicsTurnStartedTrue()) {
      htBasSettingsTurn_click();}
    }
    break;
    case 0x3B:  // bHelp
    { if (checkIsPgBasicsTurnStartedTrue()) {
      Nextion.print(F("page pgHBasicT0")); NextionEndCommand();  // goto help
      }
    }
    break;
    /*    pgAdjTurn
      40  bSet
      41  bMoveToStop
      42  bMoveToHome
      43  bSetDir
      44  bReHome
      45  bMoveCCW
      46  bStepUp
      47  bStepDown
      49  bSettingNext

      4B  htAdjBasics
      4C  htAdjSettings
      4D  bSettingPrev
      4E  bSpeedUp
      4F  bSpeedDown
      50  bSynchVideo
      67  bMoveCW

    */
    case 0x40: //  bSet
    { 
        bSetStop_click();
        set_data_on_pgAdjTurn();
    }
    break;
    case 0x41: // bMoveToStop
    {
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  
      // set vDisableStuff to disable other buttons  
      bMoveToStopTurn_click();
      set_data_on_pgAdjTurn();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  
      // set vDisableStuff to enable other buttons
    }
    break;
    case 0x42: // bMoveToHome
    {
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  
      // set vDisableStuff to disable other buttons
      bMoveToHomeTurn_click(); 
      set_data_on_pgAdjTurn();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  
      // set vDisableStuff to enable other buttons
    }
    break;
    case 0x43:  // bSetDir
    { 
        isbSetDirCW = !isbSetDirCW;
        set_direction_of_travel_iconAdjTurn();
    }
    break;
    case 0x44:  // bReHome
    {
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand(); 
      // set vDisableStuff to disable other buttons
      reHomeTurn(isbSetDirCW); 
      // tell re-home to use the direction specified by isbSetDirCW
      set_data_on_pgAdjTurn();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  
      // set vDisableStuff to enable other buttons  
    }
    break;
    case 0x45:  //bMoveCCW
    {
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  
      // set vDisableStuff to disable other buttons  
      bMoveCCW_click();
      set_data_on_pgAdjTurn();
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  
      // set vDisableStuff to enable other buttons  
    }
    break;
    case 0x46:  // bStepUp
    { cycleStepSizeTurn(1); }
    break;
    case 0x47:  // bStepDown
    { cycleStepSizeTurn(-1); }
    break;
    case 0x49:  // bSettingNext
    { bSettingNext_click(); }
    break;
    case 0x4B:  // htAdjBasics
    { htAdjBasics_click(); }
    break;
    case 0x4C:  // htAdjSettings
    { htAdjSettings_click(); }
    break;
    case 0x4D:  // bSettingPrev
    { bSettingPrev_click(); }
    break;
    case 0x4E:  // bSpeedUp
    { 
      bSpeedUpTurn_click(); 
      set_data_on_pgAdjTurn();
    }
    break;
    case 0x4F:  // bSpeedDown
    { 
      bSpeedDownTurn_click(); 
      set_data_on_pgAdjTurn();
    }
    break;
    case 0x50:  // bSynchVideo
    {
      // set vDisableStuff to disable other buttons
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  
      bSynchVideoTurn_click();
      set_data_on_pgAdjTurn();
      // set vDisableStuff to enable other buttons
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  
    }
    break;
    case 0x67:  //bMoveCW
    {
      // set vDisableStuff to disable other buttons  
      Nextion.print(F("vDisableStuff.val=1")); NextionEndCommand();  
      bMoveCW_click();
      set_data_on_pgAdjTurn();
      // set vDisableStuff to enable other buttons  
      Nextion.print(F("vDisableStuff.val=0")); NextionEndCommand();  
    }
    break;
    /*    Help Pages
      80  bReturn

    */
    case 0x80: //  bReturn
    { 
      if (strcmp(saveSettingsMotor,"C") ==0) {
        Nextion.print(F("page pgAdjustments")); NextionEndCommand();
      }
      else {
        Nextion.print(F("page pgAdjTurn")); NextionEndCommand();
      }
    }
    break;
    /*    pgHSettings0
      81  htSetBasics
      82  htSetAdj

    */
    case 0x81: //  htSetBasics
    { 
      if (strcmp(saveSettingsMotor,"C") ==0) {
        Nextion.print(F("page pgHBasic0")); NextionEndCommand();
      }
      else {
        Nextion.print(F("page pgHBasicT0")); NextionEndCommand();
      }
    }
    break;
    case 0x82: //  htSetAdj
    { 
      if (strcmp(saveSettingsMotor,"C") ==0) {
        Nextion.print(F("page pgHAdj0")); NextionEndCommand();
      }
      else {
        Nextion.print(F("page pgHAdjTurn0")); NextionEndCommand();
      }
    }
    break;
  } 

}  
   
void END_NEXTION_COMMON_ROUTINES() {}

void setup() {
  
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(turntableEnbl, OUTPUT);
  pinMode(turntableDM0, OUTPUT);
  pinMode(turntableDM1, OUTPUT);
  pinMode(turntableDM2, OUTPUT);
  
  pinMode(carriageEnbl, OUTPUT);
  pinMode(carriageDM0, OUTPUT);
  pinMode(carriageDM1, OUTPUT);
  pinMode(carriageDM2, OUTPUT);
  
  pinMode(homeCarriagePin, INPUT);
  pinMode(hallSensorPin, INPUT);

  // set carriage motor to 1/32 microstep
  digitalWrite(carriageDM0, HIGH);
  digitalWrite(carriageDM1, HIGH);   
  digitalWrite(carriageDM2, HIGH);

  // set turntable motor to correct step mode.  We always use 1/32
  digitalWrite(turntableDM0, HIGH);
  digitalWrite(turntableDM1, HIGH);
  digitalWrite(turntableDM2, HIGH);
  
  Serial.begin(9600);
  Nextion.begin(9600);  // MUST BE AFTER the Serial.begin() (if Serial.begin() is present)
  delay(500);

  mystepper.setPinsInverted(true, false, false);
   
  defaultSettings = getSystemDefaultSettings();
  EEPROM.get(eeAddress, onDiskSettings);

  if (strcmp(onDiskSettings.marker,validMarker) != 0) {
    onDiskSettings.savedSettings = defaultSettings;
    strcpy(onDiskSettings.marker, validMarker);
    EEPROM.put(eeAddress, onDiskSettings);
  }
  unSavedSettings = onDiskSettings.savedSettings;

  stepsPerTurntableRevolution = stepsPerTurntableRevolutionInFullStepMode
   * stepMultiplierTurn; 
   
  if (strcmp(unSavedSettings.startupPanel,"c") == 0){
    Nextion.print(F("page pgBasics")); NextionEndCommand(); 
    delay(100);
  }
  else {
    Nextion.print(F("page pgBasicsTurn")); NextionEndCommand(); 
    delay(100);
  }
}

void loop() {
  /*
      Valid inputs:
      # x03 P x?? x00
          Where 
          # is constant
          x03 is constant and means there are 3 bytes coming
          P is constant and means that a PAGE is being refreshed
          ?? is a 1-byte PAGE ID that uniquely identifies the page
          x00 is constant
      
      # x03 B x?? x00

        Where
          # is constant
          x03 is constant and means there are 3 bytes coming
          B is constant and means tha BUTTON has been pressed
          ?? is the 1-byte BUTTON ID that uniquely identifies the button
            ex:  x00 x01 x02 ... x13 ...
          decimal 0   1   2   ... 19 ...
          x00 is constant

      # x03 S x?? x00

        Where
          # is constant
          x03 is constant and means there are 3 bytes coming
          S is constant and means a SPECIAL ACTION has been triggeredd
          ?? is the SPECIAL CODE that uniquely identifies the use case.
             valid values are:
              01 - turntable video has completed
              02 - carriage video has completed
          x00 is constant
  */
  uint8_t parm1 =0;   uint8_t parm2 =0;   uint8_t len = 0;   uint8_t cmd = 0;   char start_char;
  
  if(Nextion.available() > 2){ 
    start_char = Nextion.read();      // place first byte into start_char 
    len = Nextion.read();      // place next byte into len
    // <len> is the number of bytes that must be received to consider the notification complete
    if(start_char == '#'){                // first byte must be # to be a valid notification
      unsigned long tmr_1 = millis();
      boolean cmd_found = true;
      while(Nextion.available() < len){ // Waiting for all the bytes that we declare with <len> to arrive              
        if((millis() - tmr_1) > 100){    // Waiting... But not forever...... 
          cmd_found = false;             // tmr_1 a timer to avoid the stack in the while loop 
          break;                         // if there is not any bytes on Serial   
        }                                     
      }                                   
      if(cmd_found == true){   // if we got a complete notification string...
        cmd = Nextion.read();  // place next byte into cmd
        if (cmd == 'P' || cmd == 'B' || cmd == 'S') {
          parm1 = Nextion.read();  // place next byte into parm1
          parm2 = Nextion.read();  // place next byte into parm2
        }
        switch (cmd){
          case 'P': // 0x50  "Page" command.
            page_refresh(parm1);  
          break;
          case 'B': // 0x42  "Button" command
            button_clicked(parm1); 
          break;
          case 'S': // 0x53  "Special" action
              if (parm1 == 0x01 && parm2 == 0x00) {  // this means turntable video has completed.
                isVideoTurnStopped = true;
                hideVideoIfItIsTimeTurn();
              }
              if (parm1 == 0x02 && parm2 == 0x00) {  // this means carriage video has completed.
                isVideoTurnStoppedCarriage = true;
                hideVideoIfItIsTimeCarriage();  
              }
          break;
        }
      }
    }  
  }  
}