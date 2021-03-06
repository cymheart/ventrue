﻿#include"VentrueCmd.h"

namespace ventrue
{
	VentrueCmd::VentrueCmd(Ventrue* ventrue)
	{
		this->ventrue = ventrue;
	}

	//处理任务
	void VentrueCmd::ProcessTask(TaskCallBack taskCallBack, void* data, int delay)
	{
		ventrue->PostTask(taskCallBack, data, delay);
	}

	MidiPlay* VentrueCmd::GetMidiPlay(int midiFileIdx)
	{
		return ventrue->GetMidiPlay(midiFileIdx);
	}

	//添加替换乐器
	void VentrueCmd::AppendReplaceInstrument(
		int orgBankMSB, int orgBankLSB, int orgInstNum,
		int repBankMSB, int repBankLSB, int repInstNum)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _AppendReplaceInstrument;
		ev->exValue[0] = orgBankMSB;
		ev->exValue[1] = orgBankLSB;
		ev->exValue[2] = orgInstNum;
		ev->exValue[3] = repBankMSB;
		ev->exValue[4] = repBankLSB;
		ev->exValue[5] = repInstNum;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_AppendReplaceInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		float* exValue = ventrueEvent->exValue;

		ventrue.AppendReplaceInstrument(
			exValue[0], exValue[1], exValue[2],
			exValue[3], exValue[4], exValue[5]);
	}

	//移除替换乐器
	void VentrueCmd::RemoveReplaceInstrument(int orgBankMSB, int orgBankLSB, int orgInstNum)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _RemoveReplaceInstrument;
		ev->exValue[0] = orgBankMSB;
		ev->exValue[1] = orgBankLSB;
		ev->exValue[2] = orgInstNum;
		ventrue->PostTask(ev);
	}

	//移除替换乐器
	void VentrueCmd::_RemoveReplaceInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		float* exValue = ventrueEvent->exValue;
		ventrue.RemoveReplaceInstrument(exValue[0], exValue[1], exValue[2]);
	}

	//// 按下按键
	void VentrueCmd::OnKey(int key, float velocity, VirInstrument* virInst)
	{

		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::OnKey;
		ev->processCallBack = _OnKey;
		ev->key = key;
		ev->velocity = velocity;
		ev->virInst = virInst;
		ventrue->PostRealtimeKeyOpTask(ev);
	}

	void VentrueCmd::_OnKey(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.OnKey(ventrueEvent->key, ventrueEvent->velocity, ventrueEvent->virInst);
	}

	// 释放按键
	void VentrueCmd::OffKey(int key, float velocity, VirInstrument* virInst)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::OffKey;
		ev->processCallBack = _OffKey;
		ev->key = key;
		ev->velocity = velocity;
		ev->virInst = virInst;
		ventrue->PostRealtimeKeyOpTask(ev);
	}

	void VentrueCmd::_OffKey(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.OffKey(ventrueEvent->key, ventrueEvent->velocity, ventrueEvent->virInst);
	}


	//增加效果器
	void VentrueCmd::AddEffect(VentrueEffect* effect)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::Unknown;
		ev->processCallBack = _AddEffect;
		ev->ptr = effect;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_AddEffect(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.AddEffect((VentrueEffect*)ventrueEvent->ptr);
	}

	// 发送文本
	void VentrueCmd::SendText(string text)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::Text;
		ev->processCallBack = _SendText;
		ev->text.assign(text);
		ev->midiFile = nullptr;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_SendText(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		cout << ventrueEvent->text << "(ms)" << endl;
	}


	// 添加Midi文件
	void VentrueCmd::AppendMidiFile(string midifile)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::AppendMidiFile;
		ev->processCallBack = _AppendMidiFile;
		ev->midiFilePath.assign(midifile);
		ev->midiFile = nullptr;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_AppendMidiFile(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.AppendMidiFile(ventrueEvent->midiFilePath);
	}


	// 载入Midi
	void VentrueCmd::LoadMidi(int idx)
	{
		thread_local Semaphore waitSem;

		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _LoadMidi;
		ev->midiFile = nullptr;
		ev->midiFileIdx = idx;
		ev->sem = &waitSem;
		ventrue->PostTask(ev);

		//
		waitSem.wait();
	}

	void VentrueCmd::_LoadMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.LoadMidi(ventrueEvent->midiFileIdx);
		ventrueEvent->sem->set();
	}


	// 播放指定编号的内部Midi文件
	void VentrueCmd::PlayMidi(int idx)
	{
		thread_local Semaphore waitSem;

		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _PlayMidi;
		ev->midiFileIdx = idx;
		ev->sem = &waitSem;
		ventrue->PostTask(ev);

		waitSem.wait();
	}

	void VentrueCmd::_PlayMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.PlayMidi(ventrueEvent->midiFileIdx);
		ventrueEvent->sem->set();
	}

	// 播放指定编号的内部Midi文件
	void VentrueCmd::StopMidi(int idx)
	{
		thread_local Semaphore waitSem;
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _StopMidi;
		ev->midiFileIdx = idx;
		ev->sem = &waitSem;
		ventrue->PostTask(ev);
		waitSem.wait();
	}

	void VentrueCmd::_StopMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.StopMidi(ventrueEvent->midiFileIdx);
		ventrueEvent->sem->set();
	}

	//暂停播放midi
	void VentrueCmd::SuspendMidi(int idx)
	{
		thread_local Semaphore waitSem;
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _SuspendMidi;
		ev->midiFileIdx = idx;
		ev->sem = &waitSem;
		ventrue->PostTask(ev);
		waitSem.wait();
	}

	void VentrueCmd::_SuspendMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.SuspendMidi(ventrueEvent->midiFileIdx);
		ventrueEvent->sem->set();
	}

	// 移除指定编号的内部Midi文件
	void VentrueCmd::RemoveMidi(int idx)
	{
		thread_local Semaphore waitSem;
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::PlayMidiIdx;
		ev->processCallBack = _RemoveMidi;
		ev->midiFileIdx = idx;
		ev->sem = &waitSem;
		ventrue->PostTask(ev);
		waitSem.wait();
	}

	void VentrueCmd::_RemoveMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.RemoveMidi(ventrueEvent->midiFileIdx);
		ventrueEvent->sem->set();
	}

	// 指定midi文件播放的起始时间点
	void VentrueCmd::MidiGoto(int idx, float sec)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::PlayMidiGoto;
		ev->processCallBack = _MidiGoto;
		ev->midiFileIdx = idx;
		ev->sec = sec;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_MidiGoto(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.MidiGoto(ventrueEvent->midiFileIdx, ventrueEvent->sec);
	}


	//为midi文件设置打击乐号
	void VentrueCmd::SetPercussionProgramNum(int midiFileIdx, int num)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _SetPercussionProgramNum;
		ev->midiFileIdx = midiFileIdx;
		ev->value = num;
		ventrue->PostTask(ev);
	}

	//为midi文件设置打击乐号
	void VentrueCmd::_SetPercussionProgramNum(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.SetPercussionProgramNum(ventrueEvent->midiFileIdx, ventrueEvent->value);
	}


	// 禁止播放指定编号Midi文件的轨道
	void VentrueCmd::DisableMidiTrack(int midiFileIdx, int trackIdx)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _DisableMidiTrack;
		ev->midiFile = nullptr;
		ev->midiFileIdx = midiFileIdx;
		ev->midiTrackIdx = trackIdx;
		ventrue->PostTask(ev);
	}

	// 禁止播放Midi的所有轨道
	void VentrueCmd::DisableAllMidiTrack(int midiFileIdx)
	{
		DisableMidiTrack(midiFileIdx, -1);
	}


	void VentrueCmd::_DisableMidiTrack(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.DisableMidiTrack(ventrueEvent->midiFileIdx, ventrueEvent->midiTrackIdx);
	}


	// 启用播放指定编号Midi文件的轨道
	void VentrueCmd::EnableMidiTrack(int midiFileIdx, int trackIdx)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _EnableMidiTrack;
		ev->midiFile = nullptr;
		ev->midiFileIdx = midiFileIdx;
		ev->midiTrackIdx = trackIdx;
		ventrue->PostTask(ev);
	}

	// 启用播放Midi的所有轨道
	void VentrueCmd::EnableAllMidiTrack(int midiFileIdx)
	{
		EnableMidiTrack(midiFileIdx, -1);
	}

	void VentrueCmd::_EnableMidiTrack(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.EnableMidiTrack(ventrueEvent->midiFileIdx, ventrueEvent->midiTrackIdx);
	}

	// 禁止播放指定编号Midi文件的轨道通道
	void VentrueCmd::DisableMidiTrackChannel(int midiFileIdx, int trackIdx, int channelIdx)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _DisableMidiTrackChannel;
		ev->midiFile = nullptr;
		ev->midiFileIdx = midiFileIdx;
		ev->midiTrackIdx = trackIdx;
		ev->value = channelIdx;
		ventrue->PostTask(ev);
	}

	// 禁止播放Midi指定轨道上的所有通道
	void VentrueCmd::DisableMidiTrackAllChannels(int midiFileIdx, int trackIdx)
	{
		DisableMidiTrackChannel(midiFileIdx, trackIdx, -1);
	}


	void VentrueCmd::_DisableMidiTrackChannel(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.DisableMidiTrackChannel(ventrueEvent->midiFileIdx, ventrueEvent->midiTrackIdx, ventrueEvent->value);

	}

	// 启用播放指定编号Midi文件的轨道通道
	void VentrueCmd::EnableMidiTrackChannel(int midiFileIdx, int trackIdx, int channelIdx)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _EnableMidiTrackChannel;
		ev->midiFile = nullptr;
		ev->midiFileIdx = midiFileIdx;
		ev->midiTrackIdx = trackIdx;
		ev->value = channelIdx;
		ventrue->PostTask(ev);
	}

	// 启用播放Midi指定轨道上的所有通道
	void VentrueCmd::EnableMidiTrackAllChannels(int midiFileIdx, int trackIdx)
	{
		EnableMidiTrackChannel(midiFileIdx, trackIdx, -1);
	}


	void VentrueCmd::_EnableMidiTrackChannel(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.EnableMidiTrackChannel(ventrueEvent->midiFileIdx, ventrueEvent->midiTrackIdx, ventrueEvent->value);
	}


	// 设置设备通道Midi控制器值
	void VentrueCmd::SetDeviceChannelMidiControllerValue(int deviceChannelNum, MidiControllerType midiController, int value)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::SetDeviceChannelMidiControllerValue;
		ev->processCallBack = _SetDeviceChannelMidiControllerValue;
		ev->deviceChannelNum = deviceChannelNum;
		ev->midiCtrlType = midiController;
		ev->value = value;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_SetDeviceChannelMidiControllerValue(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);

		Channel* channel = ventrue.GetDeviceChannel(ventrueEvent->deviceChannelNum);
		if (channel == nullptr) {
			return;
		}

		channel->SetControllerValue(ventrueEvent->midiCtrlType, ventrueEvent->value);
		ventrue.ModulationVirInstParams(channel);
	}

	// 在虚拟乐器列表中，创建新的指定虚拟乐器
	VirInstrument* VentrueCmd::NewVirInstrument(int bankSelectMSB, int bankSelectLSB, int instrumentNum)
	{
		uint64_t deviceChannelNum = UniqueID::GetInstance().gen();
		return EnableVirInstrument(deviceChannelNum, bankSelectMSB, bankSelectLSB, instrumentNum);
	}

	/// <summary>
	/// 在虚拟乐器列表中，启用指定的虚拟乐器,如果不存在将在虚拟乐器列表中自动创建它
	/// 注意如果deviceChannelNum已近被使用过，此时会直接修改这个通道上的虚拟乐器的音色到指定音色，
	/// 而不会同时在一个通道上创建超过1个的虚拟乐器
	/// </summary>
	/// <param name="deviceChannel">乐器所在的设备通道</param>
	/// <param name="bankSelectMSB">声音库选择0</param>
	/// <param name="bankSelectLSB">声音库选择1</param>
	 /// <param name="instrumentNum">乐器编号</param>
	/// <returns></returns>
	VirInstrument* VentrueCmd::EnableVirInstrument(uint32_t deviceChannelNum, int bankSelectMSB, int bankSelectLSB, int instrumentNum)
	{
		thread_local VirInstrument* inst = 0;
		thread_local Semaphore waitGetInstrumentSem;

		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::EnableInstrument;
		ev->processCallBack = _EnableInstrument;
		ev->deviceChannelNum = deviceChannelNum;
		ev->bankSelectLSB = bankSelectLSB;
		ev->bankSelectMSB = bankSelectMSB;
		ev->instrumentNum = instrumentNum;
		ev->ptr = (void*)&inst;
		ev->sem = &waitGetInstrumentSem;
		ventrue->PostTask(ev);

		//
		waitGetInstrumentSem.wait();
		return inst;
	}

	void VentrueCmd::_EnableInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		VirInstrument* vinst =
			ventrue.EnableVirInstrument(
				ventrueEvent->deviceChannelNum,
				ventrueEvent->bankSelectMSB,
				ventrueEvent->bankSelectLSB,
				ventrueEvent->instrumentNum);

		VirInstrument** threadVInst = (VirInstrument**)ventrueEvent->ptr;
		*threadVInst = vinst;
		ventrueEvent->sem->set();
	}

	/// <summary>
	/// 移除乐器
	/// </summary>
	void VentrueCmd::RemoveVirInstrument(VirInstrument* virInst, bool isFade)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _RemoveInstrument;
		ev->ptr = (void*)virInst;
		ev->value = isFade ? 1 : 0;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_RemoveInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		VirInstrument* virInst = (VirInstrument*)ventrueEvent->ptr;
		ventrue.RemoveVirInstrument(virInst, ventrueEvent->value);
	}

	/// <summary>
	/// 删除乐器
	/// </summary>
	void VentrueCmd::DelVirInstrument(VirInstrument* virInst)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _DelInstrument;
		ev->ptr = (void*)virInst;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_DelInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		VirInstrument* virInst = (VirInstrument*)ventrueEvent->ptr;
		ventrue.DelVirInstrument(virInst);
	}

	/// <summary>
	/// 打开虚拟乐器
	/// </summary>
	void VentrueCmd::OnVirInstrument(VirInstrument* virInst, bool isFade)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::EnableInstrument;
		ev->processCallBack = _OnInstrument;
		ev->ptr = (void*)virInst;
		ev->value = isFade ? 1 : 0;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_OnInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		VirInstrument* virInst = (VirInstrument*)ventrueEvent->ptr;
		ventrue.OnVirInstrument(virInst, ventrueEvent->value);
	}

	/// <summary>
	/// 关闭虚拟乐器
	/// </summary>
	void VentrueCmd::OffVirInstrument(VirInstrument* virInst, bool isFade)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::EnableInstrument;
		ev->processCallBack = _OffInstrument;
		ev->ptr = (void*)virInst;
		ev->value = isFade ? 1 : 0;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_OffInstrument(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		VirInstrument* virInst = (VirInstrument*)ventrueEvent->ptr;
		ventrue.OffVirInstrument(virInst, ventrueEvent->value);
	}

	/// <summary>
	/// 获取虚拟乐器列表的备份
	/// </summary>
	vector<VirInstrument*>* VentrueCmd::TakeVirInstrumentList()
	{
		thread_local vector<VirInstrument*>* insts = 0;
		thread_local Semaphore waitGetInstsSem;

		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->processCallBack = _TakeVirInstrumentList;
		ev->ptr = (void*)&insts;
		ev->sem = &waitGetInstsSem;

		ventrue->PostTask(ev);

		waitGetInstsSem.wait();
		return insts;
	}

	void VentrueCmd::_TakeVirInstrumentList(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		vector<VirInstrument*>** threadVInsts = (vector<VirInstrument*>**)ventrueEvent->ptr;
		*threadVInsts = ventrue.TakeVirInstrumentList();
		ventrueEvent->sem->set();
	}

	/// <summary>
	/// 录制所有乐器弹奏为midi
	/// </summary>
	/// <param name="bpm">录制的BPM</param>
	 /// <param name="tickForQuarterNote">一个四分音符发音的tick数</param>
	void VentrueCmd::RecordMidi(float bpm, float tickForQuarterNote)
	{
		RecordMidi(nullptr, bpm, tickForQuarterNote);
	}

	/// <summary>
	/// 录制指定乐器弹奏为midi
	/// </summary>
	/// <param name="virInst">如果为null,将录制所有乐器</param>
	/// <param name="bpm">录制的BPM</param>
	/// <param name="tickForQuarterNote">一个四分音符发音的tick数</param>
	void VentrueCmd::RecordMidi(VirInstrument* virInst, float bpm, float tickForQuarterNote)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::RecordMidi;
		ev->virInst = virInst;
		ev->bpm = bpm;
		ev->tickForQuarterNote = tickForQuarterNote;
		ev->processCallBack = _RecordMidi;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_RecordMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.RecordMidi(ventrueEvent->virInst, ventrueEvent->bpm, ventrueEvent->tickForQuarterNote);
	}

	/// <summary>
	/// 停止所有乐器当前midi录制
	/// </summary>
	void VentrueCmd::StopRecordMidi()
	{
		StopRecordMidi(nullptr);
	}

	/// <summary>
	/// 停止录制指定乐器弹奏midi
	/// </summary>
	/// <param name="virInst">如果为null,将录制所有乐器</param>
	void VentrueCmd::StopRecordMidi(VirInstrument* virInst)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::StopRecordMidi;
		ev->virInst = virInst;
		ev->processCallBack = _StopRecordMidi;
		ventrue->PostTask(ev);
	}


	void VentrueCmd::_StopRecordMidi(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.StopRecordMidi(ventrueEvent->virInst);
	}

	/// <summary>
	/// 生成所有乐器已录制的midi到midiflie object中
	/// </summary>
	/// <returns>midiflie object</returns>
	MidiFile* VentrueCmd::CreateRecordMidiFileObject()
	{
		return CreateRecordMidiFileObject(nullptr, 0);
	}

	/// <summary>
	/// 根据给定的乐器，生成它的已录制的midi到midiflie object中
	/// </summary>
	/// <param name="virInsts">乐器</param>
	/// <returns>midiflie object</returns>
	MidiFile* VentrueCmd::CreateRecordMidiFileObject(VirInstrument* virInst)
	{
		return CreateRecordMidiFileObject(&virInst, 1);
	}

	/// <summary>
	/// 根据给定的乐器组，生成它们已录制的midi到midiflie object中
	/// </summary>
	/// <param name="virInsts">乐器组</param>
	/// <returns>midiflie object</returns>
	MidiFile* VentrueCmd::CreateRecordMidiFileObject(VirInstrument** virInsts, int size)
	{
		thread_local MidiFile* midiFile = 0;
		thread_local Semaphore waitSem;

		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::CreateRecordMidiFileObject;
		ev->processCallBack = _CreateRecordMidiFileObject;
		ev->ptr = (void*)&midiFile;
		ev->exPtr[0] = virInsts;
		ev->exValue[0] = (float)size;
		ev->sem = &waitSem;
		ventrue->PostTask(ev);

		//
		waitSem.wait();
		return midiFile;

	}

	void VentrueCmd::_CreateRecordMidiFileObject(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		MidiFile* midiFile = ventrue.CreateRecordMidiFileObject((VirInstrument**)ventrueEvent->exPtr[0], (int)(ventrueEvent->exValue[0]));
		MidiFile** threadMidiFile = (MidiFile**)ventrueEvent->ptr;
		*threadMidiFile = midiFile;
		ventrueEvent->sem->set();
	}

	//保存midiFile到文件
	void VentrueCmd::SaveMidiFileToDisk(MidiFile* midiFile, string saveFilePath)
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = ventrue;
		ev->evType = VentrueEventType::SaveMidiFileToDisk;
		ev->midiFile = midiFile;
		ev->midiFilePath = saveFilePath;
		ev->processCallBack = _SaveMidiFileToDisk;
		ventrue->PostTask(ev);
	}

	void VentrueCmd::_SaveMidiFileToDisk(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.SaveMidiFileToDisk(ventrueEvent->midiFile, ventrueEvent->midiFilePath);
	}

}
