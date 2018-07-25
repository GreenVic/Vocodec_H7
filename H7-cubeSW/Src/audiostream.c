/* Includes ------------------------------------------------------------------*/
#include "audiostream.h"
#include "main.h"
#include "codec.h"



// align is to make sure they are lined up with the data boundaries of the cache 
// at(0x3....) is to put them in the D2 domain of SRAM where the DMA can access them
// (otherwise the TX times out because the DMA can't see the data location) -JS


int32_t audioOutBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;
int32_t audioInBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;

float detuneAmounts[NUM_OSC];

int16_t inBuffer[HALF_BUFFER_SIZE];
int16_t outBuffer[HALF_BUFFER_SIZE];

uint16_t* adcVals;

uint8_t buttonAPressed = 0;

uint8_t doAudio = 0;

float sample = 0.0f;

float adcx[8];

float detuneMax = 3.0f;
uint8_t audioInCV = 0;
uint8_t audioInCVAlt = 0;
float myVol = 0.0f;

float audioTickL(float audioIn); 
float audioTickR(float audioIn);
void buttonCheck(void);





HAL_StatusTypeDef transmit_status;
HAL_StatusTypeDef receive_status;

typedef enum BOOL {
	FALSE = 0,
	TRUE
} BOOL;

void audioInit(I2C_HandleTypeDef* hi2c, SAI_HandleTypeDef* hsaiOut, SAI_HandleTypeDef* hsaiIn, RNG_HandleTypeDef* hrand, uint16_t* myADCArray)
{ 
	// Initialize the audio library. OOPS.
	OOPSInit(SAMPLE_RATE, AUDIO_FRAME_SIZE, &randomNumber);

	//now to send all the necessary messages to the codec
	AudioCodec_init(hi2c);

	HAL_Delay(100);

	adcVals = myADCArray;
	for (int i = 0; i < NUM_OSC; i++)
	{
		osc[i] = tSawtoothInit();
		tSawtoothSetFreq(osc[i], (randomNumber() * 2000.0f) + 40.0f);
		detuneAmounts[i] = (randomNumber() * detuneMax) - (detuneMax * 0.5f);
	}

	// set up the I2S driver to send audio data to the codec (and retrieve input as well)	
	transmit_status = HAL_SAI_Transmit_DMA(hsaiOut, (uint8_t *)&audioOutBuffer[0], AUDIO_BUFFER_SIZE);
	receive_status = HAL_SAI_Receive_DMA(hsaiIn, (uint8_t *)&audioInBuffer[0], AUDIO_BUFFER_SIZE);


}

float tempVal = 0.0f;
uint16_t frameCounter = 0;
int32_t sampleCounter = 0;
void audioFrame(uint16_t buffer_offset)
{
	uint16_t i = 0;
	int32_t current_sample = 0;

	for (int j = 0; j < NUM_OSC; j++)
	{
		{
			tSawtoothSetFreq(osc[j], (float)((adcVals[j % 4] >> 6) + 200)+ detuneAmounts[j]);
		}
	}

	for (i = 0; i < (HALF_BUFFER_SIZE); i++)
	{

		if ((i & 1) == 0)
		{
			current_sample = (int32_t)(audioTickL((float) (audioInBuffer[buffer_offset + i] * INV_TWO_TO_31)) * TWO_TO_31);
		}
		else
		{
			//current_sample = (int32_t)(audioTickR((float) (audioInBuffer[buffer_offset + i] * INV_TWO_TO_31)) * TWO_TO_31);

		}
		audioOutBuffer[buffer_offset + i] = current_sample;
	}
}

float currentFreq = 1.0f;

float rightInput = 0.0f;



float audioTickL(float audioIn) 
{


	sample = 0.0f;

	for (int i = 0; i < NUM_OSC; i++)
	{
		sample += tSawtoothTick(osc[i]);
	}

	sample *= 0.0625f;
	sample = tanhf(sample); //nice soft clipper - probably expensive but seems to be OK

	//use the foot pedal as a volume control!
	sample *= ((float)adcVals[4]) * INV_TWO_TO_16;

	return sample;
}

float audioTickR(float audioIn)
{
	rightInput = audioIn;
	//sample = audioIn;
	return audioIn;
}

void buttonCheck(void)
{
	buttonValues[0] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
	buttonValues[1] = !HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);
	buttonValues[2] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);
	buttonValues[3] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12);

	for (int i = 0; i < 4; i++)
	{
	  if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] < 40))
	  {
		  buttonCounters[i]++;
	  }
	  if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] >= 40))
	  {
		  if (buttonValues[i] == 1)
		  {
			  buttonPressed[i] = 1;
		  }
		  buttonValuesPrev[i] = buttonValues[i];
		  buttonCounters[i] = 0;
	  }
	}

	if (buttonPressed[0] == 1)
	{
		//HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
		buttonPressed[0] = 0;

	}
	if (buttonPressed[1] == 1)
	{
		buttonPressed[1] = 0;
	}
	if (buttonPressed[2] == 1)
	{
		buttonPressed[2] = 0;
	}

	if (buttonPressed[3] == 1)
	{
		buttonPressed[3] = 0;
	}
}


void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
	//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
	;
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  ;
}


void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(HALF_BUFFER_SIZE);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(0);
}

