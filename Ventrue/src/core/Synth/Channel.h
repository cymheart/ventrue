﻿#ifndef _Channel_h_
#define _Channel_h_

#include"VentrueTypes.h"
#include"Midi/MidiTypes.h"
#include"MidiTrackRecord.h"

namespace ventrue
{
	class Channel
	{

	public:
		Channel(Track* track, uint64_t channelNum);
		~Channel();

		//获取通道号
		inline uint64_t GetChannelNum()
		{
			return channelNum;
		}

		//获取通道所在轨道
		inline Track* GetTrack()
		{
			return track;
		}

		void Clear();

		inline int GetBankSelectMSB()
		{
			return bankMSB;
		}

		inline int GetBankSelectLSB()
		{
			return bankLSB;
		}

		inline int GetProgramNum()
		{
			return programNum;
		}

		//手动选择乐器
		void SelectProgram(int bankMSB, int bankLSB, int instNum);

		//设置乐器
		void SetProgramNum(int num);

		void SetControllerValue(MidiControllerType type, int value);
		int GetControllerValue(MidiControllerType type);

		float GetModPresetValue(ModInputPreset presetType);

		///获取控制器计算值
		float GetControllerComputedValue(MidiControllerType type);

		MidiControllerTypeList& GetUsedControllerTypeList();
		ModPresetTypeList& GetUsedPresetTypeList();

		//设置滑音
		void SetPitchBend(int value);

		//获取滑音范围
		inline float GetPitchBendRange()
		{
			return pitchBendRange;
		}

		//设置midi录制类
		inline void SetMidiRecord(MidiTrackRecord* midiTrackRecord)
		{
			this->midiTrackRecord = midiTrackRecord;
		}

		//是否禁止播放
		inline bool IsDisablePlay()
		{
			return isDisablePlay;
		}

		//禁止播放
		inline void DiablePlay()
		{
			isDisablePlay = true;
		}

		//开启播放
		inline void EnablePlay()
		{
			isDisablePlay = false;
		}

	private:

		void ComputeControllerHighResValue(int type);
		float MapValueToSys(MidiControllerType type, float value, bool isHighResValue);

		//增加使用的控制器类型
		void AddUsedControllerType(MidiControllerType type);

		//增加使用的预设类型
		void AddUsedPresetType(ModInputPreset type);

	public:


	private:
		//通道号
		uint64_t channelNum = 0;
		//所在轨道
		Track* track = nullptr;

		//
		int bankMSB = 0;
		int bankLSB = 0;
		int programNum = 0;

		//
		int ccValue[128] = { 0 };
		float ccCombValue[128] = { 0 };
		float ccComputedValue[128] = { 0 };
		float pitchBend = 0;
		float pitchBendRange = 2;
		float fineTune = 0;
		float coarseTune = 0;

		//是否禁止播放
		bool isDisablePlay = false;

		MidiControllerTypeList usedControllerTypeList;
		ModPresetTypeList usedPresetTypeList;


		//
		MidiTrackRecord* midiTrackRecord = nullptr;

	};
}

#endif
