﻿#include"Track.h"
#include"Channel.h"

namespace ventrue
{
	Track::Track(int num)
	{
		this->num = num;

		for (int i = 0; i < 16; i++)
			channels[i] = new Channel(this, i);

		channels[9]->SetControllerValue(MidiControllerType::BankSelectMSB, 128);
		channels[9]->SetControllerValue(MidiControllerType::BankSelectLSB, 0);
		channels[9]->SetProgramNum(0);
	}

	Track::~Track()
	{
		for (int i = 0; i < 16; i++) {
			DEL(channels[i]);
		}
	}

	void Track::Clear()
	{
		msPerTick = 4.166f;
		BPM = 120;
		eventOffsetIdx = 0;
		curtTickCount = 0;
		baseTickCount = 0;
		baseTickTime = 0;
		isEnded = false;

		for (int i = 0; i < 16; i++)
			channels[i]->Clear();

		channels[9]->SetControllerValue(MidiControllerType::BankSelectMSB, 128);
		channels[9]->SetControllerValue(MidiControllerType::BankSelectLSB, 0);
		channels[9]->SetProgramNum(0);
	}

	Channel* Track::operator[] (int n)
	{
		return channels[n];
	}

	//禁止播放通道
	void Track::DisablePlayChannel(int n)
	{
		if (n >= 16)
			return;

		channels[n]->DiablePlay();
	}

	//开启播放通道
	void Track::EnablePlayChannel(int n)
	{
		if (n >= 16)
			return;

		channels[n]->EnablePlay();
	}

	/// <summary>
	/// 设置轨道速度
	/// </summary>
	/// <param name="microTempo">一个四分音符的微秒数</param>
	/// <param name="tickForQuarterNote">一个四分音符的tick数</param>
	/// <param name="startTick">开始设置速度的tick数</param>
	void Track::SetTempo(float microTempo, float tickForQuarterNote, int startTickCount)
	{
		baseTickTime = GetTickSec(startTickCount);
		msPerTick = microTempo / tickForQuarterNote * 0.001f;
		BPM = 60000000 / microTempo;  //60000000: 1分钟的微秒数
		baseTickCount = startTickCount;
	}

	/// <summary>  
	/// 根据当前时间点,计算已经过的tick的数量  
	/// sec以秒为单位
	/// </summary>    
	void Track::CalCurtTicksCount(double sec)
	{
		curtTickCount = GetTickCount(sec);
	}

	//根据给定时间点获取tick的数量
	uint32_t Track::GetTickCount(double sec)
	{
		return baseTickCount + (uint32_t)((sec - baseTickTime) * 1000 / msPerTick);
	}

	//根据给定tick数量获取时间点
	double Track::GetTickSec(int tickCount)
	{
		return baseTickTime + (tickCount - baseTickCount) * msPerTick / 1000;
	}
}
