﻿#ifndef _VirInstrument_h_
#define _VirInstrument_h_

#include"VentrueTypes.h"
#include"Midi/MidiEvent.h"
#include"Midi/MidiTrack.h"
#include"MidiTrackRecord.h"

namespace ventrue
{
	/*
	* 虚拟乐器类
	* 通过指定通道和预设来确认当前乐器所在的演奏空间
	* 这个类主要用来处理同为一个空间的一种乐器的一组按键发音器(KeySounder)的方法结合
	* by cymheart, 2020--2021.
	*/
	class DLL_CLASS VirInstrument
	{
	public:
		VirInstrument(Ventrue* ventrue, Channel* channel, Preset* preset);
		~VirInstrument();

		//获取乐器所在通道
		Channel* GetChannel()
		{
			return channel;
		}


	private:

		//获取Ventrue
		Ventrue* GetVentrue()
		{
			return ventrue;
		}

		//获取乐器的预设
		Preset* GetPreset()
		{
			return preset;
		}

		//增加效果器
		void AddEffect(VentrueEffect* effect);

		// 获取左声道已处理采样数据
		float* GetLeftChannelSamples()
		{
			return leftChannelSamples;
		}

		// 获取右声道已处理采样数据
		float* GetRightChannelSamples()
		{
			return rightChannelSamples;
		}

		//所有keySounder发音是否结束
		bool IsAllKeySoundEnd();

		//乐器是否发声结束
		inline bool IsSoundEnd()
		{
			return isSoundEnd;
		}

		//最后一个保留按键
		inline KeySounder* GetLastKeySounder()
		{
			return lastKeySounder;
		}

		//移除指定按下状态的按键发音器
		bool _RemoveOnKeyStateSounder(KeySounder* keySounder);

		//移除指定按下状态的按键发音器
		void RemoveOnKeyStateSounder(KeySounder* keySounder);

		// 获取最后在按状态的按键发音器(不包括最后松开的按键)
		KeySounder* GetLastOnKeyStateSounder();

		// 在Mono模式下重新发音最后一个按键
		void MonoModeReSoundLastOnKey();

		// 获取指定的实际按下的按键的KeySounder
		KeySounder* GetOnKeyStateSounder(KeySounderID keySounderID);

		//从KeySounders中查找KeySounder
		bool FindKeySounderFromKeySounders(KeySounder* keySounder);

		// 找寻最后一个按键发声区域中具有同样乐器区域的regionSounder
		RegionSounder* FindLastSameRegion(Region* region);

		//是否使用单音模式
		bool UseMonoMode()
		{
			return useMonoMode;
		}

		// 获取是否总是使用滑音   
		bool AlwaysUsePortamento()
		{
			return alwaysUsePortamento;
		}

		//获取滑音过渡时间
		float GetPortaTime()
		{
			return portaTime;
		}

		//是否使用连音
		bool UseLegato()
		{
			return useLegato;
		}

		//调制生成器参数
		void ModulationParams();

		// 移除已完成所有区域发声处理(采样处理)的KeySounder   
		void RemoveProcessEndedKeySounder();

		//清除通道样本缓存
		void ClearChannelSamples();

		//应用效果器到乐器的声道buffer
		void ApplyEffectsToChannelBuffer();


		void SetPreset(Preset* preset)
		{
			this->preset = preset;
		}

		//设置滑音过渡时间
		void SetPortaTime(float tm)
		{
			portaTime = tm;
		}

		//设置是否使用连音
		void SetUseLegato(bool isUse)
		{
			useLegato = isUse;
		}

		//设置是否使用单音模式  
		void SetUseMonoMode(bool isUse)
		{
			useMonoMode = isUse;
		}

		// 设置是否总是使用滑音    
		void SetAlwaysUsePortamento(bool isAlwaysUse)
		{
			alwaysUsePortamento = isAlwaysUse;
		}


		//设置区域混音深度
		void SetRegionReverbDepth(float value);

		//设置和声深度
		void SetRegionChorusDepth(float value);

		//按键
		KeySounder* OnKey(int key, float velocity);

		//松开按键
		void OffKey(int key, float velocity = 127.0f);

		// 松开指定发音按键
		void OffKey(KeySounder* keySounder, float velocity = 127.0f);

		// 执行松开指定发音按键
		void OffKey(KeySounderID keySounderID, float velocity = 127.0f);

		// 松开指定发音按键
		void _OffKey(KeySounder* keySounder, float velocity, bool isRecord);

		//按键执行
		void _OnKey(KeySounder* keySounder, int key, float velocity);

		// 设置具有相同独占类的区域将不再处理样本
		// exclusiveClasses数组以一个小于等于0的值结尾
		void StopExclusiveClassRegionSounderProcess(int* exclusiveClasses);

		/// <summary>
		/// 录制指定乐器弹奏为midi
		/// </summary>
		/// <param name="bpm">录制的BPM</param>
		/// <param name="tickForQuarterNote">一个四分音符发音的tick数</param>
		void RecordMidi(float bpm, float tickForQuarterNote);

		//停止录制midi
		void StopRecordMidi();

		// 获取录制的midi轨道
		MidiTrack* TakeMidiTrack(float baseTickForQuarterNote);

		//合并区域已处理发音样本
		void CombineRegionSounderSamples(RegionSounder* regionSounder);

		//为渲染准备所有正在发声的区域
		int CreateRegionSounderForRender(RegionSounder** totalRegionSounder, int startSaveIdx);

	private:

		//发音结束,包括效果器作用带来的尾音是否结束
		bool isSoundEnd = true;

		//效果器
		EffectList* effects;
		//是否启用效果器
		bool isEnableEffect = true;

		//区域混音深度
		float regionReverbDepth = 0;
		// 是否激活了区域混音处理
		bool isActiveRegionReverb = false;
		// 区域混音处理
		EffectReverb* regionReverb = nullptr;

		//区域和声深度
		float regionChorusDepth = 0;
		// 是否激活了区域和声处理
		bool isActiveRegionChorus = false;
		// 区域和声处理
		EffectChorus* regionChorus = nullptr;

		// 使用单音模式  
		bool useMonoMode = false;

		// 是否总是使用滑音
		bool alwaysUsePortamento = false;

		// 使用连音   
		bool useLegato = false;

		//滑音过渡时间
		float portaTime = 0;

		Ventrue* ventrue = nullptr;

		//乐器所在的通道
		Channel* channel = nullptr;

		//乐器所在预设
		Preset* preset = nullptr;

		// 发音中的按键发声器
		KeySounderList* keySounders = nullptr;

		//按键中的keySounders
		vector<KeySounder*>* onKeySounders = nullptr;

		// 左通道已处理采样点
		float leftChannelSamples[8192] = { 0 };

		// 右通道已处理采样点
		float rightChannelSamples[8192] = { 0 };

		//最后一个松开按键的键
		//最后一个保持按键状态的keySounder,总是不被直接从内存池中移除，
		//而是放入lastKeySounder，供总是保持滑音状态使用，因为“保持滑音状态”
		//需要最后一个发音作参考，来生成滑音,正常状态的滑音是靠两个同时处于按键状态的keySounder先后来生成的
		//当开起一直保持功能后，生成滑音只需要最近一个按键的发音信息即可，而无所谓是否按下还是已经按下过
		KeySounder* lastKeySounder = nullptr;

		int exclusiveClasses[128] = { 0 };
		KeySounder* offKeySounder[256];

		//midiTrack录制
		MidiTrackRecord* midiTrackRecord;

		//
		friend class Ventrue;
		friend class RegionSounderThread;
		friend class MidiPlay;
		friend class KeySounder;
		friend class RegionSounder;
	};
}

#endif
