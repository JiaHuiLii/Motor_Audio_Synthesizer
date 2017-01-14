#include "AudioPluginUtil.h"

namespace NoiseBox2
{
    enum Param
    {
        P_ADDAMT,
        P_ADDFREQ,

        P_MasterGain,
        P_LowGain,
        P_MidGain,
        P_HighGain,
        P_LowFreq,
        P_MidFreq,
        P_HighFreq,
      
        P_RunTime,
        P_StatorLevel,
        P_BrushLevel,
        P_RotorLevel,
        P_MaxSpeed,
        P_Volume,
        
        P_TestEnv,
        
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            float addcount;
            float addnoise;
            float phasor;
            
            BiquadFilter FilterH[8];
            BiquadFilter FilterP[8];
            BiquadFilter FilterL[8];
            BiquadFilter DisplayFilterCoeffs[3];
            float sr;
            float s;
            float c;
            Random random;

        };
        union
        {
            Data data;
            unsigned char pad[(sizeof(Data) + 15) & ~15]; // This entire structure must be a multiple of 16 bytes (and and instance 16 byte aligned) for PS3 SPU DMA requirements
        };
    };

#if !UNITY_SPU

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        RegisterParameter(definition, "Add Amount", "dB", -100.0f, 0.0f, -50.0f, 1.0f, 1.0f, P_ADDAMT, "Gain of additive noise in dB");
        //RegisterParameter(definition, "Mul Amount", "dB", -100.0f, 0.0f, -50.0f, 1.0f, 1.0f, P_MULAMT, "Gain of multiplicative noise in dB");
        RegisterParameter(definition, "Add Frequency", "Hz", 0.001f, 24000.0f, 5000.0f, 1.0f, 3.0f, P_ADDFREQ, "Additive noise frequency cutoff in Hz");
        //RegisterParameter(definition, "Mul Frequency", "Hz", 0.001f, 24000.0f, 50.0f, 1.0f, 3.0f, P_MULFREQ, "Multiplicative noise frequency cutoff in Hz");

        RegisterParameter(definition, "MasterGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_MasterGain, "Overall gain applied");
        RegisterParameter(definition, "LowGain", "dB", -100.0f, 100.0f, -50.0f, 1.0f, 1.0f, P_LowGain, "Gain applied to lower frequency band");
        RegisterParameter(definition, "MidGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_MidGain, "Gain applied to middle frequency band");
        RegisterParameter(definition, "HighGain", "dB", -100.0f, 100.0f, -50.0f, 1.0f, 1.0f, P_HighGain, "Gain applied to high frequency band");
        RegisterParameter(definition, "LowFreq", "Hz", 0.01f, 24000.0f, 800.0f, 1.0f, 3.0f, P_LowFreq, "Cutoff frequency of lower frequency band");
        RegisterParameter(definition, "MidFreq", "Hz", 0.01f, 24000.0f, 4000.0f, 1.0f, 3.0f, P_MidFreq, "Center frequency of middle frequency band");
        RegisterParameter(definition, "HighFreq", "Hz", 0.01f, 24000.0f, 8000.0f, 1.0f, 3.0f, P_HighFreq, "Cutoff frequency of high frequency band");

        
        RegisterParameter(definition, "RunTime", "ms", 0.0f, 20000.0f, 10000.0f, 1.0f, 1.0f, P_RunTime, "Overall time");
        RegisterParameter(definition, "StatorLevel", "", 0.0f, 1.0f, 0.8f, 1.0f, 1.0f, P_StatorLevel, "A pulse-like wave from the stator vibration");
        RegisterParameter(definition, "BrushLevel", "", 0.0f, 1.0f, 0.4f, 1.0f, 1.0f, P_BrushLevel, "Noise bursts from the brushes");
        RegisterParameter(definition, "RotorLevel", "", 0.0f, 1.0f, 0.4f, 1.0f, 1.0f, P_RotorLevel, "Click-like sound from the rotor");
        RegisterParameter(definition, "MaxSpeed", "", 0.0f, 1.0f, 0.7f, 1.0f, 1.0f, P_MaxSpeed, "Speed");
        RegisterParameter(definition, "P_Volume", "dB", 0.0f, 1.0f, 0.7f, 1.0f, 1.0f, P_Volume, "Volume");
        
         RegisterParameter(definition, "TestEnv", "", 0.0f, 1.0f, 0.8f, 1.0f, 1.0f, P_TestEnv, "Env for Test");
        return numparams;
        
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        effectdata->data.phasor = 0.0f;  //set phasor valur initial to 0
        effectdata->data.c = 1.0f;

        //effectdata->data.vline = 0.0f;  //set vline value initial to 0
        state->effectdata = effectdata;
        InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
//
#if !UNITY_PS3
        data->analyzer.Cleanup();
#endif
        //
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }
//
    
    static void SetupFilterCoeffs(EffectData::Data* data, BiquadFilter* filterH, BiquadFilter* filterP, BiquadFilter* filterL, float samplerate)
    {
        filterH->SetupHighShelf(data->p[P_HighFreq], samplerate, data->p[P_HighGain], 1);
        filterP->SetupPeaking(data->p[P_MidFreq], samplerate, data->p[P_MidGain], 1);
        filterL->SetupLowShelf(data->p[P_LowFreq], samplerate, data->p[P_LowGain], 1);
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        
#if !UNITY_PS3
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (strcmp(name, "InputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, true);
        else if (strcmp(name, "OutputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, false);
        else if (strcmp(name, "Coeffs") == 0)
        {
            SetupFilterCoeffs(data, &data->DisplayFilterCoeffs[0], &data->DisplayFilterCoeffs[1], &data->DisplayFilterCoeffs[2], (float)state->samplerate);
//            data->DisplayFilterCoeffs[2].StoreCoeffs(buffer);
//            data->DisplayFilterCoeffs[1].StoreCoeffs(buffer);
//            data->DisplayFilterCoeffs[0].StoreCoeffs(buffer);
        }
        else
#endif
            memset(buffer, 0, sizeof(float) * numsamples);
        return UNITY_AUDIODSP_OK;
    }

#endif

#if !UNITY_PS3 || UNITY_SPU

#if UNITY_SPU
    EffectData  g_EffectData __attribute__((aligned(16)));
    extern "C"
#endif

    
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;

#if UNITY_SPU
        UNITY_PS3_CELLDMA_GET(&g_EffectData, state->effectdata, sizeof(g_EffectData));
        data = &g_EffectData.data;
#endif
        
        float sr = (float)state->samplerate;
        for (int i = 0; i < inchannels; i++)
            SetupFilterCoeffs(data, &data->FilterH[i], &data->FilterP[i], &data->FilterL[i], sr);
        
#if !UNITY_PS3 && !UNITY_SPU

#endif
        
        if ((state->flags & UnityAudioEffectStateFlags_IsPlaying) == 0 || (state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused)) != 0)
        {
            memcpy(outbuffer, inbuffer, sizeof(float) * length * inchannels);
            return UNITY_AUDIODSP_OK;
        }

        const float addperiod = state->samplerate * 0.5f / data->p[P_ADDFREQ];
        //const float mulperiod = state->samplerate * 0.5f / data->p[P_MULFREQ];
        const float addgain = powf(10.0f, 0.05f * data->p[P_ADDAMT]);
        //const float mulgain = powf(10.0f, 0.05f * data->p[P_MULAMT]);
        const float masterGain = powf(10.0f, data->p[P_MasterGain] * 0.05f);
        float statordriver = 0;
        float statorfinal = 0; //stator output
        
        float w = (data->p[P_MaxSpeed]*(2000)*data->p[P_TestEnv]/state->samplerate);  //set env's value to 2000, add it while calculate the sawtooth
    
        for (unsigned int n = 0; n < length; n++)
        {
            
            for (int i = 0; i < outchannels; i++)
            {
                
                
                inbuffer[n * inchannels + i] = inbuffer[n * inchannels + i] + addgain * data->addnoise;
                
                float killdenormal = (float)(data->random.Get() & 255) * 1.0e-9f;
                float y = inbuffer[n * inchannels + i] + killdenormal;
                y = data->FilterH[i].Process(y);
                y = data->FilterP[i].Process(y);
                y = data->FilterL[i].Process(y)*masterGain;    //noise and filter
                
                
                statordriver = data->phasor * 2;     //start to calculate stator,roter
                if (statordriver>1){ statordriver -= 1;}
                
                statorfinal =( 1 / (pow( cosf(2*kPI*statordriver), 2) + 1) - 0.5 ) * data->p[P_StatorLevel];
                //statorfinal =( 1 / (pow( data->c, 2) + 1) - 0.5 ) * data->p[P_StatorLevel];  //calculate the output of stator
                
                
                float rotorFinal = (y * data->p[P_BrushLevel] + data->p[P_RotorLevel]);
                rotorFinal = rotorFinal * pow(data->phasor,4);
                
                //final
                outbuffer[n * outchannels + i] = (rotorFinal+ statorfinal) * data->p[P_TestEnv] * data->p[P_Volume]; //rotor multiply to phasor，then modulate by env and volume
              
            }
            data->addcount += 1.0f; if (data->addcount >= addperiod) { data->addcount -= addperiod; data->addnoise = data->random.GetFloat(-1.0, 1.0f); }
           // data->mulcount += 1.0f; if (data->mulcount >= addperiod) { data->mulcount -= mulperiod; data->mulnoise = data->random.GetFloat(0.0, 1.0f); }

            
            float countcos = 2.0f * sinf( kPI * statordriver / state->samplerate);  //calculte the cos and sin
            data->s += data->c * countcos; // calcualte stator
            data->c -= data->s * countcos;
            
            data->phasor += w;     //calculate sawtooth
            if(data->phasor > 1){ data->phasor = 0;}
            
        }
        
        
#if UNITY_SPU
        UNITY_PS3_CELLDMA_PUT(&g_EffectData, state->effectdata, sizeof(g_EffectData));
#endif

        return UNITY_AUDIODSP_OK;
    }

#endif
}
