

#include <Stepper.h>
#include "Encoder.h"

#include "FiniteStateMachine.h"
#include "FBD.h"
// stage structure, I think we can add more varibles such as speed in each step etc
typedef struct
{
	uint32_t nStartCnt;
	uint32_t nStopCnt;
} Stage;

// stage series, we can predefine these series.
#define STAGECNT 6
static Stage stages[] = {
	{0, 600}, {600, 7200}, {7200, 14400}, {14400, 21600}, {21600, 28288}, {28288, 0}};

/*************  ********/
#define FORWARD 0
#define REVERSE 1

// Main Controller Class defintion
class StepController
{
public:
	StepController();
	uint8_t currStageNum;
	uint8_t direction; //
	Stage *currStage;

	void displayStatus();
	void nextStage();
	void gotoStart();
	void gotoEnd();
};

StepController::StepController()
{
	direction = FORWARD;
	currStageNum = 0;
	currStage = stages;
}

void StepController::nextStage()
{

	currStageNum++;
	if (currStageNum == STAGECNT)
	{
		currStageNum = 0;
		currStage = stages;
	}
	else
	{
		currStage++;
	}
}

void StepController::displayStatus()
{
	Serial.println(F(""));
	Serial.println(F("*******************"));
	Serial.print(F("Current Stage number is "));
	Serial.println(currStageNum);

	Serial.print(F("start count is "));
	Serial.println(currStage->nStartCnt);

	Serial.print(F("end count is "));
	Serial.println(currStage->nStopCnt);

	if ((currStage->nStopCnt - currStage->nStartCnt) > 0)
	{
		direction = REVERSE;
	}

	if (direction)
		Serial.println(F("forward direction"));
	else
		Serial.println(F("reverse direction"));
	Serial.println(F("*******************"));
}

void StepController::gotoStart()
{
	currStageNum = 0;
	currStage = stages;
}

void StepController::gotoEnd()
{
	currStageNum = STAGECNT - 1;
	currStage = &stages[STAGECNT - 1];
}

/************************************/

/***************** 
GPIO definitions, if wiring is changed we have to modify them
*****************/
#define TURNON HIGH
#define TURNOFF LOW

// motor enable pins defintion
const uint8_t ENA = 5;
const uint8_t ENB = 6;

// stepper motor instacnes
const int stepsPerRevolution = 200;
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);

//
const uint8_t CHNA = 2;
const uint8_t CHNB = 3;

// quadrature optical encoder
Encoder encoder(CHNA, CHNB);

// remote LED
const uint8_t REMOTELED = A0;

// rem
const uint8_t REMOTEBUTTON = 4;

// edge micro switches
const uint8_t STARTEDGE = A1;
const uint8_t ENDEDGE = A2;

/**************************/

// FBD objects for input debounce
TON buttonTON, startTON, endTON;
Rtrg buttonTrg, startTrg, endTrg;

// soft debounce time for button and micro switches
const uint16_t signalDebounce = 100;

//
void initFBDs()
{
	buttonTON.IN = false;
	buttonTON.Q = false;
	buttonTON.PT = signalDebounce;
	buttonTON.ET = millis();

	buttonTrg.IN = false;
	buttonTrg.Q = false;
	buttonTrg.PRE = false;

	startTON.IN = false;
	startTON.Q = false;
	startTON.PT = signalDebounce;
	startTON.ET = millis();

	startTrg.IN = false;
	startTrg.Q = false;
	startTrg.PRE = false;

	endTON.IN = false;
	endTON.Q = false;
	endTON.PT = signalDebounce;
	endTON.ET = millis();

	endTrg.IN = false;
	endTrg.Q = false;
	endTrg.PRE = false;
}

// func procs for state machine
void idleEnter();
void idleUpdate();

void stageEnter();
void stageUpdate();

State idle = State(idleEnter, idleUpdate, NULL);
State movingStage = State(stageEnter, stageUpdate, NULL);
FSM controller = FSM(idle);

StepController stepController;

void setup()
{

	Serial.begin(9600);
	Serial.println(F("Program Started..."));
	delay(1000);

	//
	pinMode(ENA, OUTPUT);
	pinMode(ENB, OUTPUT);

	//
	pinMode(STARTEDGE, INPUT_PULLUP);
	pinMode(ENDEDGE, INPUT_PULLUP);
	pinMode(REMOTEBUTTON, INPUT_PULLUP);

	//
	pinMode(REMOTELED, OUTPUT);
	digitalWrite(REMOTELED, TURNOFF);
	initFBDs();

	// enable motors
	analogWrite(ENA, 200);
	analogWrite(ENB, 200);

	// set the speed at 60 rpm:
	myStepper.setSpeed(60);
}

void loop()
{
	// get button & switches status
	buttonTON.IN = digitalRead(REMOTEBUTTON) == LOW;
	TONFunc(&buttonTON);
	buttonTrg.IN = buttonTON.Q;
	RTrgFunc(&buttonTrg);

	startTON.IN = digitalRead(STARTEDGE) == LOW;
	TONFunc(&startTON);
	startTrg.IN = startTON.Q;
	RTrgFunc(&startTrg);

	endTON.IN = digitalRead(ENDEDGE) == LOW;
	TONFunc(&endTON);
	endTrg.IN = endTON.Q;
	RTrgFunc(&endTrg);

	controller.update();

	// LED Control as status
	if (controller.isInState(idle))
		digitalWrite(REMOTELED, TURNON);
	else
		digitalWrite(REMOTELED, TURNOFF);
}

/**********************************************/

void idleEnter()
{
	Serial.println(F("entered into idle status"));
	// diable motors
	analogWrite(ENA, 0);
	analogWrite(ENB, 0);
}

void idleUpdate()
{
	if (buttonTrg.Q)
	{ // button triggered
		Serial.println(F("remote button is triggered"));
		controller.transitionTo(movingStage);
	}
}

void stageEnter()
{
	// reset encoder
	encoder.write(0);

	// enable motors
	analogWrite(ENA, 200);
	analogWrite(ENB, 200);

	//
	Serial.println(F("entered into stage moving status."));

	stepController.displayStatus();
}

void stageUpdate()
{
	int32_t stageEncCnt = stepController.currStage->nStopCnt - stepController.currStage->nStartCnt;
	if (stageEncCnt > 0)
	{
		// forward mode
		myStepper.step(1);
	}
	else
	{
		// reverse mode
		myStepper.step(-1);
	}

	//
	uint32_t currEncCount = encoder.read();
	const uint32_t delta = 2;
	if (currEncCount > (abs(stageEncCnt) - delta) && currEncCount < (abs(stageEncCnt) + delta))
	{
		Serial.println(F("arrived at specified position!"));
		stepController.nextStage();
		controller.transitionTo(idle);
	}

	else if (startTrg.Q)
	{
		Serial.println(F("start edge is detected"));
		stepController.gotoStart();
		controller.transitionTo(idle);
	}
	else if (endTrg.Q)
	{
		Serial.println(F("end edge is detected"));
		stepController.gotoEnd();
		controller.transitionTo(idle);
	}
}
