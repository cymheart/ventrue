﻿#include"Ventrue.h"
#include"Sample.h"
#include"RegionSounderThread.h"
#include"Instrument.h"
#include"KeySounder.h"
#include"VentrueEvent.h"
#include"SoundFormat\Wav\WavReader.h"
#include"Midi/MidiFile.h"
#include"MidiPlay.h"
#include"Preset.h"
#include"RegionSounder.h"
#include"VirInstrument.h"
#include"SoundFontParser.h"
#include"SoundFontFormat/SF2/SF2Parser.h"
#include"SoundFontFormat/VentrueFont/VentrueFont.h"
#include"Channel.h"
#include"VentrueCmd.h"
#include "Audio/AudioSDL/Audio_SDL.h"
#include "Audio/AudioOboe/Audio_oboe.h"
#include"dsignal/Bode.h"
#include"Effect/EffectEqualizer.h"
#include <Effect\EffectCompressor.h>
#include<algorithm>


using namespace dsignal;

/*
* Ventrue是一个soundfont合成器
* by cymheart, 2020--2021.
*/
namespace ventrue {

	Ventrue::Ventrue()
	{
		cmd = new VentrueCmd(this);
		openedAudioTime = new clock::time_point;
		isFrameRenderCompleted = true;
		cmdLock = new mutex();
		waitSem = new Semaphore();
		sampleList = new SampleList;
		instList = new InstrumentList;
		presetList = new PresetList;
		presetBankDict = new PresetMap;
		presetBankReplaceMap = new unordered_map<uint32_t, uint32_t>;
		virInstList = new vector<VirInstrument*>;
		virInsts = new vector<VirInstrument*>;
		taskProcesser = new TaskProcesser;
		realtimeKeyOpTaskProcesser = new TaskProcesser;
		realtimeKeyEventList = new RealtimeKeyEventList;

		regionSounderThreadPool = new RegionSounderThread;
		regionSounderThreadPool->SetVentrue(this);


#ifdef _WIN32
		audio = new Audio_SDL();
#else
		audio = new Audio_oboe();
#endif

		audio->SetAudioCallback(FillAudioSample, this);

		//synthSampleRingBuffer = new RingBuffer(1024 * 4 * 2 * 40);

		sfParserMap = new SoundFontParserMap();
		AddSoundFontParsers();

		//
		midiFilePaths = new vector<string>();
		midiPlayMap = new unordered_map<int32_t, MidiPlay*>;

		//
		deviceChannelMap = new ChannelMap();

		//
		effects = new EffectList();
		effects->Set(leftChannelSamples, rightChannelSamples, frameSampleCount);
		EffectCompressor* compressor = new EffectCompressor();
		effects->AppendEffect(compressor);

#ifdef _WIN32
		timeBeginPeriod(1);
#endif

		taskProcesser->Start();
		realtimeKeyOpTaskProcesser->Start();
	}

	Ventrue::~Ventrue()
	{
		DEL(cmd);

		//注意，必须首先停止audio的回调运作
		DEL(audio);
		DEL(synthSampleRingBuffer);
		regionSounderThreadPool->Stop();
		taskProcesser->Stop();
		realtimeKeyOpTaskProcesser->Stop();

		//
		DEL_OBJS_VECTOR(sampleList);
		DEL_OBJS_VECTOR(instList);
		DEL_OBJS_VECTOR(presetList);
		DEL(midiFilePaths);

		DEL(virInsts);
		DEL(presetBankDict);
		DEL(presetBankReplaceMap);
		DEL(deviceChannelMap);
		DEL_OBJS_VECTOR(virInstList);
		DEL(realtimeKeyEventList);
		DEL(openedAudioTime);

		//
		DEL(taskProcesser);
		DEL(realtimeKeyOpTaskProcesser);
		DEL(regionSounderThreadPool);

		//
		DEL(effects);

		DEL(cmdLock);
		DEL(waitSem);

		//
		for (auto iter = midiPlayMap->begin(); iter != midiPlayMap->end(); ++iter)
		{
			DEL(iter->second);
		}
		DEL(midiPlayMap);

		//
		SoundFontParserMap::iterator it = sfParserMap->begin();
		for (; it != sfParserMap->end(); it++)
			DEL(it->second);
		DEL(sfParserMap);


#ifdef _WIN32
		timeEndPeriod(1);
#endif
	}

	//投递任务
	void Ventrue::PostTask(TaskCallBack taskCallBack, void* data, int delay)
	{
		taskProcesser->PostTask(taskCallBack, data, delay);
	}

	//投递任务
	void Ventrue::PostTask(Task* task, int delay)
	{
		taskProcesser->PostTask(task, delay);
	}

	//投递实时按键操作任务
	void Ventrue::PostRealtimeKeyOpTask(Task* task, int delay)
	{
		realtimeKeyOpTaskProcesser->PostTask(task, delay);
	}

	//设置声道模式(立体声，单声道设置)
	void Ventrue::SetChannelOutputMode(ChannelOutputMode outputMode)
	{
		channelOutputMode = outputMode;
		audio->SetChannelCount((int)channelOutputMode);
	}

	//设置帧样本数量
	//这个值越小，声音的实时性越高（在实时演奏时，值最好在1024以下，最合适的值为512）,
	//当这个值比较小时，cpu内耗增加
	void Ventrue::SetFrameSampleCount(int count)
	{
		frameSampleCount = count;
		if (frameSampleCount < 256) frameSampleCount = 256;
		audio->SetSampleCount(frameSampleCount);

		if (childFrameSampleCount > frameSampleCount) childFrameSampleCount = frameSampleCount;
		else if (childFrameSampleCount < 1)childFrameSampleCount = 1;

		effects->Set(leftChannelSamples, rightChannelSamples, frameSampleCount);
	}


	void Ventrue::Close()
	{
		regionSounderThreadPool->Stop();
		taskProcesser->Stop();
	}

	/// <summary>
	/// 开启声音播放引擎
	/// </summary>
	void Ventrue::OpenAudio()
	{
		*openedAudioTime = clock::now();
		audio->SetFreq((int)sampleProcessRate);
		//首次开启先渲染一帧，让ringbuffer中的写入值大于读取值
		Render();
		audio->Open();
	}

	void Ventrue::FillAudioSample(void* udata, uint8_t* stream, int len)
	{
		((Ventrue*)udata)->FrameRender(stream, len);
	}

	//添加midi文件
	void Ventrue::AppendMidiFile(string midiFilePath)
	{
		midiFilePaths->push_back(midiFilePath);
	}

	//载入midi
	void Ventrue::LoadMidi(int idx)
	{
		if (idx >= midiFilePaths->size())
			return;

		auto it = midiPlayMap->find(idx);
		if (it != midiPlayMap->end())
			return;

		//
		MidiPlay* midiPlay = new MidiPlay(this);
		midiPlay->ParseMidiFile((*midiFilePaths)[idx], GetTrackChannelMergeMode());
		(*midiPlayMap)[idx] = midiPlay;
	}

	//播放midi
	void Ventrue::PlayMidi(int idx)
	{
		auto it = midiPlayMap->find(idx);
		if (it != midiPlayMap->end()) {
			it->second->Play();
		}
	}

	//停止播放midi
	void Ventrue::StopMidi(int idx)
	{
		auto it = midiPlayMap->find(idx);
		if (it != midiPlayMap->end()) {
			it->second->Stop();
		}
	}

	//暂停播放midi
	void Ventrue::SuspendMidi(int idx)
	{
		auto it = midiPlayMap->find(idx);
		if (it != midiPlayMap->end()) {
			it->second->Suspend();
		}
	}

	//移除midi
	void Ventrue::RemoveMidi(int idx)
	{
		auto it = midiPlayMap->find(idx);
		if (it == midiPlayMap->end())
			return;

		it->second->Remove();
		DEL(it->second);
		midiPlayMap->erase(idx);
	}

	//midi快进到指定位置
	void Ventrue::MidiGoto(int idx, float sec)
	{
		auto it = midiPlayMap->find(idx);
		if (it != midiPlayMap->end()) {
			it->second->Goto(sec);
		}
	}

	// 禁止播放指定编号Midi文件的轨道
	void Ventrue::DisableMidiTrack(int idx, int trackIdx)
	{
		auto it = midiPlayMap->find(idx);
		if (it == midiPlayMap->end())
			return;

		if (trackIdx == -1)
			it->second->DisableAllTrack();
		else
			it->second->DisableTrack(trackIdx);
	}

	// 启用播放指定编号Midi文件的轨道
	void Ventrue::EnableMidiTrack(int idx, int trackIdx)
	{
		auto it = midiPlayMap->find(idx);
		if (it == midiPlayMap->end())
			return;

		if (trackIdx == -1)
			it->second->EnableAllTrack();
		else
			it->second->EnableTrack(trackIdx);
	}

	// 禁止播放指定编号Midi文件的轨道通道
	void Ventrue::DisableMidiTrackChannel(int idx, int trackIdx, int channelIdx)
	{
		auto it = midiPlayMap->find(idx);
		if (it == midiPlayMap->end())
			return;

		if (channelIdx == -1)
			it->second->DisableTrackAllChannels(trackIdx);
		else
			it->second->DisableTrackChannel(trackIdx, channelIdx);
	}

	// 启用播放指定编号Midi文件的轨道通道
	void Ventrue::EnableMidiTrackChannel(int idx, int trackIdx, int channelIdx)
	{
		auto it = midiPlayMap->find(idx);
		if (it == midiPlayMap->end())
			return;

		if (channelIdx == -1)
			it->second->EnableTrackAllChannels(trackIdx);
		else
			it->second->EnableTrackChannel(trackIdx, channelIdx);
	}

	//为midi文件设置打击乐号
	void Ventrue::SetPercussionProgramNum(int idx, int num)
	{
		auto it = midiPlayMap->find(idx);
		if (it == midiPlayMap->end())
			return;

		it->second->SetPercussionProgramNum(num);
	}

	MidiPlay* Ventrue::GetMidiPlay(int idx)
	{
		auto it = midiPlayMap->find(idx);
		if (it != midiPlayMap->end())
			return it->second;
		return nullptr;
	}


	//增加所有soundfont解析器
	void Ventrue::AddSoundFontParsers()
	{
		AddSoundFontParser("VentrueFont", new VentrueFont(this));
		AddSoundFontParser("SF2", new SF2Parser(this));
	}

	//增加一个解析格式类型
	void Ventrue::AddSoundFontParser(string formatName, SoundFontParser* sfParser)
	{
		(*sfParserMap)[formatName] = sfParser;
	}

	//根据格式类型,解析soundfont文件
	void Ventrue::ParseSoundFont(string formatName, string path)
	{
		SoundFontParser* sfParser = nullptr;
		auto it = sfParserMap->find(formatName);
		if (it != sfParserMap->end())
			sfParser = it->second;

		if (sfParser)
			sfParser->Parse(path);
	}


	//设置是否使用多线程
	//使用多线程渲染处理声音
	//多线程渲染在childFrameSampleCount比较小的情况下(比如小于64时)，由于在一大帧中线程调用太过频繁，线程切换的消耗大于声音渲染的时间
	//当childFrameSampleCount >= 256时，多线程效率会比较高
	//在播放midi音乐时，并不适合开启多线程，因为midi播放事件要求一个非常小的childFrameSampleCount值
	//但在测试同时发音数量很高的midi音乐时，多线程的效率非常高，播放也稳定
	void Ventrue::SetUseMulThread(bool use)
	{
		useMulThreads = use;
		if (use)
		{
			regionSounderThreadPool->Start();
		}
		else
		{
			regionSounderThreadPool->Stop();
		}
	}

	// 设置样本处理采样率
	void Ventrue::SetSampleProcessRate(int rate)
	{
		sampleProcessRate = (float)rate;
		invSampleProcessRate = 1 / sampleProcessRate;
		Stk::setSampleRate(sampleProcessRate);
	}

	// 增加一个样本到样本列表
	Sample* Ventrue::AddSample(string name, short* samples, size_t size, byte* sm24)
	{
		Sample* sample = new Sample();
		sample->name = name;
		sample->SetSamples(samples, (uint32_t)size, sm24);
		sampleList->push_back(sample);
		return sample;
	}

	// 增加一个乐器到乐器列表
	Instrument* Ventrue::AddInstrument(string name)
	{
		Instrument* inst = new Instrument();
		inst->name = name;
		instList->push_back(inst);
		return inst;
	}

	// 增加一个预设到预设列表
	Preset* Ventrue::AddPreset(string name, int bankSelectMSB, int bankSelectLSB, int instrumentNum)
	{
		Preset* preset = new Preset();
		preset->name = name;
		preset->SetBankNum(bankSelectMSB, bankSelectLSB, instrumentNum);
		presetList->push_back(preset);
		(*presetBankDict)[preset->GetBankKey()] = preset;
		return preset;
	}

	// 乐器绑定到预设上
	Region* Ventrue::InstrumentBindToPreset(Instrument* inst, Preset* preset)
	{
		return preset->LinkInstrument(inst);
	}

	// 样本绑定到乐器上
	Region* Ventrue::SampleBindToInstrument(Sample* sample, Instrument* inst)
	{
		return inst->LinkSamples(sample);
	}

	// 根据指定通道获取关连虚拟乐器
	VirInstrument* Ventrue::GetVirInstrumentByChannel(Channel* channel)
	{
		if (channel == nullptr)
			return nullptr;

		VirInstrument* virInst = nullptr;
		for (int i = 0; i < virInstList->size(); i++)
		{
			if ((*virInstList)[i]->GetChannel() == channel)
			{
				virInst = (*virInstList)[i];
				break;
			}
		}

		return virInst;
	}


	// 在虚拟乐器列表中，创建新的指定虚拟乐器
	VirInstrument* Ventrue::NewVirInstrument(int bankSelectMSB, int bankSelectLSB, int instrumentNum)
	{
		uint64_t deviceChannelNum = UniqueID::GetInstance().gen();
		return EnableVirInstrument(deviceChannelNum, bankSelectMSB, bankSelectLSB, instrumentNum);
	}

	/// <summary>
	/// 在虚拟乐器列表中，启用指定的虚拟乐器,如果不存在将在虚拟乐器列表中自动创建它
	/// </summary>
	/// <param name="deviceChannel">乐器所在设备通道</param>
	/// <param name="bankSelectMSB">声音库选择0</param>
	/// <param name="bankSelectLSB">声音库选择1</param>
	/// <param name="instrumentNum">乐器编号</param>
	/// <returns></returns>
	VirInstrument* Ventrue::EnableVirInstrument(uint64_t deviceChannelNum, int bankSelectMSB, int bankSelectLSB, int instrumentNum)
	{
		Preset* preset = GetInstrumentPreset(bankSelectMSB, bankSelectLSB, instrumentNum);
		Channel* channel = GetDeviceChannel(deviceChannelNum);
		if (channel == nullptr)
		{
			channel = new Channel(nullptr, deviceChannelNum);
			channel->SelectProgram(bankSelectMSB, bankSelectLSB, instrumentNum);
			(*deviceChannelMap)[deviceChannelNum] = channel;
		}


		return EnableVirInstrument(preset, channel);
	}


	// 在虚拟乐器列表中，启用指定的虚拟乐器,如果不存在，将在虚拟乐器列表中自动创建它
	// 注意如果channel已经被使用过，此时会直接修改这个通道上的虚拟乐器的音色到指定音色，
	// 而不会同时在一个通道上创建超过1个的虚拟乐器
	VirInstrument* Ventrue::EnableVirInstrument(Preset* preset, Channel* channel)
	{
		VirInstrument* virInst = nullptr;
		for (int i = 0; i < virInstList->size(); i++)
		{
			virInst = (*virInstList)[i];

			if (!virInst->IsRemove() &&
				virInst->GetChannel() == channel)
			{
				break;
			}
			else {
				virInst = nullptr;
			}
		}

		if (virInst == nullptr)
		{
			virInst = new VirInstrument(this, channel, preset);
			virInst->OnExecute(false);
			SetVirInstRelationValues(virInst);
			virInstList->push_back(virInst);
			virInsts->push_back(virInst);
		}
		else
		{
			Preset* virInstPreset = virInst->GetPreset();
			if (virInstPreset != preset)
			{
				virInst->SetPreset(preset);
				channel->SelectProgram(preset->bankSelectMSB, preset->bankSelectLSB, preset->instrumentNum);
			}
		}

		return virInst;
	}


	//根据通道移除虚拟乐器
	void Ventrue::RemoveVirInstrumentByChannel(Channel* channel, bool isFade)
	{
		VirInstrument* virInst = nullptr;
		for (int i = 0; i < virInstList->size(); i++)
		{
			if ((*virInstList)[i]->GetChannel() == channel &&
				!(*virInstList)[i]->IsRemove())
			{
				virInst = (*virInstList)[i];
				break;
			}
		}

		if (virInst == nullptr)
			return;

		RemoveVirInstrument(virInst, isFade);
	}


	//移除虚拟乐器
	void Ventrue::RemoveVirInstrument(VirInstrument* virInst, bool isFade)
	{
		if (virInst == nullptr)
			return;

		try
		{
			virInst->Remove(isFade);
		}
		catch (exception e)
		{
			string error(e.what());
			cout << "移除乐器错误原因:" << error << endl;
		}
	}

	// 删除虚拟乐器
	void Ventrue::DelVirInstrument(VirInstrument* virInst)
	{
		if (virInst == nullptr)
			return;

		vector<VirInstrument*>::iterator it = virInstList->begin();
		for (; it != virInstList->end(); it++)
		{
			if (*it == virInst) {
				virInstList->erase(it);
				break;
			}
		}

		it = virInsts->begin();
		for (; it != virInsts->end(); it++)
		{
			if (*it == virInst) {
				virInsts->erase(it);
				break;
			}
		}


		//移除对应的设备通道
		if (virInst->GetType() == VirInstrumentType::DeviceType)
		{
			Channel* channel = virInst->GetChannel();
			uint64_t num = channel->GetChannelNum();
			channel->SetMidiRecord(nullptr);

			auto itc = deviceChannelMap->find(num);
			if (itc != deviceChannelMap->end())
			{
				deviceChannelMap->erase(itc);
				DEL(channel);
			}
		}

		//
		DEL(virInst);
	}

	//打开虚拟乐器
	void Ventrue::OnVirInstrument(VirInstrument* virInst, bool isFade)
	{
		if (virInst == nullptr)
			return;

		virInst->On(isFade);
	}

	//根据通道打开相关虚拟乐器
	void Ventrue::OnVirInstrumentByChannel(Channel* channel, bool isFade)
	{
		VirInstrument* virInst = nullptr;
		for (int i = 0; i < virInstList->size(); i++)
		{
			if ((*virInstList)[i]->GetChannel() == channel)
			{
				virInst = (*virInstList)[i];
				break;
			}
		}

		if (virInst == nullptr)
			return;

		OnVirInstrument(virInst, isFade);
	}


	//关闭虚拟乐器
	void Ventrue::OffVirInstrument(VirInstrument* virInst, bool isFade)
	{
		if (virInst == nullptr)
			return;

		virInst->Off(isFade);
	}

	//根据通道关闭相关虚拟乐器
	void Ventrue::OffVirInstrumentByChannel(Channel* channel, bool isFade)
	{
		VirInstrument* virInst = nullptr;
		for (int i = 0; i < virInstList->size(); i++)
		{
			if ((*virInstList)[i]->GetChannel() == channel)
			{
				virInst = (*virInstList)[i];
				break;
			}
		}

		if (virInst == nullptr)
			return;

		OffVirInstrument(virInst, isFade);
	}


	//根据通道关闭相关虚拟乐器所有按键
	void Ventrue::OffVirInstrumentAllKeysByChannel(Channel* channel, bool isRealTime)
	{
		VirInstrument* virInst = nullptr;
		for (int i = 0; i < virInstList->size(); i++)
		{
			if ((*virInstList)[i]->GetChannel() == channel)
			{
				virInst = (*virInstList)[i];
				break;
			}
		}

		if (virInst == nullptr)
			return;

		virInst->OffAllKeys(isRealTime);
	}


	//获取虚拟乐器列表的备份
	vector<VirInstrument*>* Ventrue::TakeVirInstrumentList()
	{
		vector<VirInstrument*>* virInsts = new vector<VirInstrument*>();
		for (int i = 0; i < virInstList->size(); i++)
			virInsts->push_back((*virInstList)[i]);
		return virInsts;
	}

	//根据设备通道号获取设备通道
	//查找不到将返回空值
	Channel* Ventrue::GetDeviceChannel(uint32_t deviceChannelNum)
	{
		auto it = deviceChannelMap->find(deviceChannelNum);
		if (it != deviceChannelMap->end()) {
			return it->second;
		}

		return nullptr;
	}

	//设置所有虚拟乐器关联值
	void Ventrue::SetVirInstsRelationValues()
	{
		for (int i = 0; i < virInstList->size(); i++)
		{
			SetVirInstRelationValues((*virInstList)[i]);
		}
	}

	//设置虚拟乐器关联值
	void Ventrue::SetVirInstRelationValues(VirInstrument* virInst)
	{
		virInst->SetAlwaysUsePortamento(alwaysUsePortamento);
		virInst->SetUsePortamento(usePortamento);
		virInst->SetUseLegato(useLegato);
		virInst->SetUseMonoMode(useMonoMode);
		virInst->SetPortaTime(portaTime);
	}

	// 设置是否总是使用滑音    
	void Ventrue::SetAlwaysUsePortamento(bool isAlwaysUse)
	{
		if (isAlwaysUse == true)
			usePortamento = isAlwaysUse;

		alwaysUsePortamento = isAlwaysUse;
		SetVirInstsRelationValues();
	}

	// 设置是否使用滑音    
	void Ventrue::SetUsePortamento(bool isUse)
	{
		usePortamento = isUse;
		SetVirInstsRelationValues();
	}

	//设置是否使用单音模式  
	void Ventrue::SetUseMonoMode(bool isUse)
	{
		useMonoMode = isUse;
		SetVirInstsRelationValues();
	}

	//设置滑音过渡时间
	void Ventrue::SetPortaTime(float tm)
	{
		portaTime = tm;
		SetVirInstsRelationValues();
	}

	//设置是否使用连音
	void Ventrue::SetUseLegato(bool isUse)
	{
		useLegato = isUse;
		SetVirInstsRelationValues();
	}

	//获取当前距离音频开启的时间
	float Ventrue::GetCurtAudioTime()
	{
		clock::time_point curTime = clock::now();
		float curtTime = (float)(chrono::duration_cast<res>(curTime - *openedAudioTime).count() * 0.000001f);
		return curtTime;
	}

	//增加效果器
	void Ventrue::AddEffect(VentrueEffect* effect)
	{
		effects->AppendEffect(effect);
	}

	//按下按键
	void Ventrue::OnKey(int key, float velocity, VirInstrument* virInst)
	{
		RealtimeKeyEvent ev;
		ev.isOnKey = true;
		ev.key = key;
		ev.velocity = velocity;
		ev.virInst = virInst;
		ev.timeSec = GetCurtAudioTime();

		cmdLock->lock();

		if (realtimeKeyEventList->empty())
		{
			ev.sampleSec = sec;
		}
		else
		{
			RealtimeKeyEvent& lastev = realtimeKeyEventList->back();
			ev.sampleSec = lastev.sampleSec + ev.timeSec - lastev.timeSec;
		}

		realtimeKeyEventList->push_back(ev);

		cmdLock->unlock();
	}

	// 释放按键
	void Ventrue::OffKey(int key, float velocity, VirInstrument* virInst)
	{
		RealtimeKeyEvent ev;
		ev.isOnKey = false;
		ev.key = key;
		ev.velocity = velocity;
		ev.virInst = virInst;
		ev.timeSec = GetCurtAudioTime();

		cmdLock->lock();

		if (realtimeKeyEventList->empty())
		{
			ev.sampleSec = sec;
		}
		else
		{
			RealtimeKeyEvent& lastev = realtimeKeyEventList->back();
			ev.sampleSec = lastev.sampleSec + ev.timeSec - lastev.timeSec;
		}

		realtimeKeyEventList->push_back(ev);
		cmdLock->unlock();

	}

	/// <summary>
	/// 录制所有乐器弹奏为midi
	/// </summary>
	/// <param name="bpm">录制的BPM</param>
	/// <param name="tickForQuarterNote">一个四分音符发音的tick数</param>
	void Ventrue::RecordMidi(float bpm, float tickForQuarterNote)
	{
		RecordMidi(nullptr, bpm, tickForQuarterNote);
	}

	/// <summary>
	/// 录制指定乐器弹奏为midi
	/// </summary>
	/// <param name="virInst">如果为null,将录制所有乐器</param>
	/// <param name="bpm">录制的BPM</param>
	/// <param name="tickForQuarterNote">一个四分音符发音的tick数</param>
	void Ventrue::RecordMidi(VirInstrument* virInst, float bpm, float tickForQuarterNote)
	{
		if (virInst != nullptr) {
			virInst->RecordMidi(bpm, tickForQuarterNote);
			return;
		}

		for (int i = 0; i < virInstList->size(); i++)
		{
			(*virInstList)[i]->RecordMidi(bpm, tickForQuarterNote);
		}
	}

	/// 停止所有乐器midi的录制
	void Ventrue::StopRecordMidi()
	{
		StopRecordMidi(nullptr);
	}

	/// <summary>
	/// 停止指定乐器midi的录制
	/// </summary>
	/// <param name="virInst">如果为null,将停止录制所有乐器</param>
	void Ventrue::StopRecordMidi(VirInstrument* virInst)
	{
		if (virInst != nullptr) {
			virInst->StopRecordMidi();
			return;
		}

		for (int i = 0; i < virInstList->size(); i++)
		{
			(*virInstList)[i]->StopRecordMidi();
		}
	}

	/// <summary>
	/// 生成所有乐器已录制的midi到midiflie object中
	/// </summary>
	/// <returns>midiflie object</returns>
	MidiFile* Ventrue::CreateRecordMidiFileObject()
	{
		return CreateRecordMidiFileObject(nullptr, 0);
	}

	/// <summary>
	/// 根据给定的乐器组，生成它们已录制的midi到midiflie object中
	/// </summary>
	/// <param name="virInsts">乐器组</param>
	/// <returns>midiflie object</returns>
	MidiFile* Ventrue::CreateRecordMidiFileObject(VirInstrument** virInsts, int size)
	{
		MidiFile* midiFile = new MidiFile();
		midiFile->SetFormat(MidiFileFormat::SyncTracks);
		midiFile->SetTickForQuarterNote(recordMidiTickForQuarterNote);

		//
		MidiTrackRecord globalMidiRecord;
		globalMidiRecord.SetBPM(recordMidiBPM);
		globalMidiRecord.SetTickForQuarterNote(recordMidiTickForQuarterNote);
		globalMidiRecord.Start();
		globalMidiRecord.Stop();
		midiFile->AddMidiTrack(globalMidiRecord.TakeMidiTrack(recordMidiTickForQuarterNote));

		MidiTrack* midiTrack;

		if (virInsts != nullptr && size > 0)
		{
			for (int i = 0; i < size; i++)
			{
				midiTrack = virInsts[i]->TakeMidiTrack(recordMidiTickForQuarterNote);
				if (midiTrack == nullptr)
					continue;

				midiFile->AddMidiTrack(midiTrack);
			}
		}
		else
		{
			for (int i = 0; i < virInstList->size(); i++)
			{
				midiTrack = (*virInstList)[i]->TakeMidiTrack(recordMidiTickForQuarterNote);
				if (midiTrack == nullptr)
					continue;

				midiFile->AddMidiTrack(midiTrack);
			}
		}

		return midiFile;
	}

	//保存midiFile到文件
	void Ventrue::SaveMidiFileToDisk(MidiFile* midiFile, string saveFilePath)
	{
		if (midiFile == nullptr)
			return;

		midiFile->CreateMidiFormatMemData();
		midiFile->SaveMidiFormatMemDataToDist(saveFilePath);
	}

	//添加替换乐器
	void Ventrue::AppendReplaceInstrument(
		int orgBankMSB, int orgBankLSB, int orgInstNum,
		int repBankMSB, int repBankLSB, int repInstNum)
	{
		int orgKey = orgBankMSB << 16 | orgBankLSB << 8 | orgInstNum;
		int repKey = repBankMSB << 16 | repBankLSB << 8 | repInstNum;
		(*presetBankReplaceMap)[orgKey] = repKey;
	}

	//移除替换乐器
	void Ventrue::RemoveReplaceInstrument(int orgBankMSB, int orgBankLSB, int orgInstNum)
	{
		int orgKey = orgBankMSB << 16 | orgBankLSB << 8 | orgInstNum;
		presetBankReplaceMap->erase(orgKey);
	}

	// 获取乐器预设
	Preset* Ventrue::GetInstrumentPreset(int bankSelectMSB, int bankSelectLSB, int instrumentNum)
	{
		int key = bankSelectMSB << 16 | bankSelectLSB << 8 | instrumentNum;

		auto itReplace = presetBankReplaceMap->find(key);
		if (itReplace != presetBankReplaceMap->end()) {
			key = itReplace->second;
		}

		auto it = presetBankDict->find(key);
		if (it != presetBankDict->end()) {
			return it->second;
		}

		//
		key &= 0xff00ff;  //bankSelectMSB << 16 | instrumentNum;
		it = presetBankDict->find(key);
		if (it != presetBankDict->end()) {
			//cout << "使用乐器:"<<instrumentNum<<" " << it->second->name << endl;
			return it->second;
		}

		return nullptr;
	}


	//调制虚拟乐器参数
	void Ventrue::ModulationVirInstParams(Channel* channel)
	{
		VirInstList::iterator it = virInstList->begin();
		VirInstList::iterator end = virInstList->end();
		for (; it != end; it++)
		{
			VirInstrument& inst = *(*it);
			if (inst.GetChannel() == channel)
				inst.ModulationParams();
		}
	}

	/// <summary>
	/// 帧渲染
	/// 当frameSampleCount过大时，audio()正在发声时，此时Render渲染线程的渲染速度高于audio线程，
	/// render线程提前停止工作
	/// 如果在此期间发送onkey, 紧接着发送offkey操作，由于render线程还未启动
	/// (如果算法中必须等待audio线程发送渲染指令才启动)，这个onkey操作将不会发出声音
	/// 修正算法: 调用ProcessRealtimeKeyEvents()处理
	/// </summary>
	/// <param name="stream"></param>
	/// <param name="len"></param>
	void Ventrue::FrameRender(uint8_t* stream, int len)
	{
		while (true)
		{
			if (isFrameRenderCompleted) {

				//synthSampleRingBuffer->ReadToDst(stream, len);
				memcpy(stream, synthSampleStream, len);
				ReqFrameRender();
				return;
			}
		}
	}

	// 请求帧渲染事件
	void Ventrue::ReqFrameRender()
	{
		VentrueEvent* ev = VentrueEvent::New();
		ev->ventrue = this;
		ev->evType = VentrueEventType::Render;
		ev->processCallBack = _FrameRender;
		isFrameRenderCompleted = false;
		taskProcesser->PostTask(ev);
	}

	void Ventrue::_FrameRender(Task* ev)
	{
		VentrueEvent* ventrueEvent = (VentrueEvent*)ev;
		Ventrue& ventrue = *(ventrueEvent->ventrue);
		ventrue.Render();
		ventrue.isFrameRenderCompleted = true;
	}


	// 渲染每帧音频
	void Ventrue::Render()
	{
		//清除通道buffer
		ClearChannelBuffer();

		//
		for (childFramePos = 0; childFramePos < frameSampleCount; childFramePos += childFrameSampleCount)
		{
			curtSampleCount += childFrameSampleCount;
			sec = invSampleProcessRate * curtSampleCount;

			if (renderTimeCallBack != nullptr)
				renderTimeCallBack(sec, renderTimeCallBackData);

			ProcessRealtimeKeyEvents();
			ProcessMidiEvents();

			//渲染虚拟乐器区域发声
			RenderVirInstRegionSound();

			//移除已完成采样的KeySounder
			RemoveProcessEndedKeySounder();

		}

		//混合所有乐器中的样本到ventrue的声道buffer中
		MixVirInstsSamplesToChannelBuffer();

		//对声道应用效果器
		ApplyEffectsToChannelBuffer();

		//合并声道buffer到数据流
		CombineChannelBufferToStream();

	}

	// 处理实时onkey或者offkey事件
	void Ventrue::ProcessRealtimeKeyEvents()
	{
		cmdLock->lock();

		if (realtimeKeyEventList->empty()) {
			cmdLock->unlock();
			return;
		}

		RealtimeKeyEventList::iterator it = realtimeKeyEventList->begin();
		for (; it != realtimeKeyEventList->end(); )
		{
			RealtimeKeyEvent& ev = *it;
			if (ev.sampleSec > sec)
				break;

			if (ev.isOnKey)
			{
				ev.virInst->OnKey(ev.key, ev.velocity);
			}
			else
			{
				ev.virInst->OffKey(ev.key, ev.velocity);
			}

			it = realtimeKeyEventList->erase(it);
		}

		cmdLock->unlock();
	}

	// 处理播放midi文件事件
	void Ventrue::ProcessMidiEvents()
	{
		MidiPlay* midiPlay;
		auto end = midiPlayMap->end();
		for (auto iter = midiPlayMap->begin(); iter != end; ++iter)
		{
			midiPlay = iter->second;
			midiPlay->TrackRun(sec);
		}
	}

	//清除所有乐器的通道buffer
	void Ventrue::ClearChannelBuffer()
	{
		memset(leftChannelSamples, 0, sizeof(float) * frameSampleCount);
		memset(rightChannelSamples, 0, sizeof(float) * frameSampleCount);

		isVirInstSoundEnd = true;
		for (int i = 0; i < virInstList->size(); i++)
		{
			VirInstrument& virInst = *(*virInstList)[i];
			if (virInst.IsSoundEnd())
				continue;

			virInst.ClearChannelSamples();
		}
	}


	// 渲染虚拟乐器区域发声
	void Ventrue::RenderVirInstRegionSound()
	{

		//为渲染准备所有正在发声的区域
		totalRegionSounderCount = 0;
		for (int i = 0; i < virInstList->size(); i++)
		{
			(*virInstList)[i]->CreateKeySounders();

			totalRegionSounderCount +=
				(*virInstList)[i]->CreateRegionSounderForRender(
					totalRegionSounders, totalRegionSounderCount);
		}

		//快速释音超过限制的区域发声
		FastReleaseRegionSounders();

		//printf("声音总数:%d\n", totalRegionSounderCount);
		//开始渲染区域声音
		if (totalRegionSounderCount <= 0)
			return;

		//是否使用线程池并行处理按键发音数据
		//多线程渲染在childFrameSampleCount比较小的情况下(比如小于64时)，由于在一大帧(frameSampleCount)中调用太过频繁，效率并不是太好
		//当childFrameSampleCount >= 256时，多线程效率会比较高
		//在播放midi音乐时，并不适合开启多线程，因为midi播放事件要求一个非常小的childFrameSampleCount值
		//但在测试同时发音数量很高的midi音乐时，多线程的效率非常高，播放也稳定
		if (!useMulThreads)
		{
			for (int i = 0; i < totalRegionSounderCount; i++)
			{
				totalRegionSounders[i]->SetFrameBuffer(leftChannelFrameBuf, rightChannelFrameBuf);
				totalRegionSounders[i]->Render();
				totalRegionSounders[i]->GetVirInstrument()->CombineRegionSounderSamples(totalRegionSounders[i]);
			}
		}
		else
		{
			regionSounderThreadPool->Render(totalRegionSounders, totalRegionSounderCount);
			regionSounderThreadPool->Wait();
		}
	}

	//快速释音超过限制的区域发声
	void Ventrue::FastReleaseRegionSounders()
	{
		int instSize = virInsts->size();
		if (instSize <= 0)
			return;

		if (instSize > 1) {
			//按乐器当前区域发声数量由少到多排列乐器
			sort(virInsts->begin(), virInsts->end(), SounderCountCompare);
		}

		int limitTotalCount = limitRegionSounderCount;
		int limitPerElemCount = limitTotalCount / instSize;
		VirInstrument* inst;
		int count;
		RegionSounder** regionSounders;
		for (int i = 0; i < instSize; i++)
		{
			inst = (*virInsts)[i];
			count = inst->GetRegionSounderCount();
			if (count <= limitPerElemCount) {
				limitTotalCount -= count;
				limitPerElemCount = limitTotalCount / (instSize - i);
				continue;
			}

			regionSounders = inst->GetRegionSounders();


			for (int j = limitPerElemCount; j < count; j++)
			{
				regionSounders[j]->OffKey(127, 0.003);
			}
			limitTotalCount -= limitPerElemCount;
			limitPerElemCount = limitTotalCount / (instSize - i);
		}
	}


	bool Ventrue::SounderCountCompare(VirInstrument* a, VirInstrument* b)
	{
		int regionCountA = a->GetRegionSounderCount();
		int regionCountB = b->GetRegionSounderCount();
		return regionCountA < regionCountB;//升序
	}


	//混合所有乐器中的样本到ventrue的声道buffer中
	void Ventrue::MixVirInstsSamplesToChannelBuffer()
	{
		isVirInstSoundEnd = true;
		for (int i = 0; i < virInstList->size(); i++)
		{
			VirInstrument& virInst = *(*virInstList)[i];
			if (virInst.IsSoundEnd())
				continue;

			isVirInstSoundEnd = false;
			isSoundEnd = false;
			virInst.ApplyEffectsToChannelBuffer();

			float* instLeftChannelSamples = virInst.GetLeftChannelSamples();
			float* instRightChannelSamples = virInst.GetRightChannelSamples();

			switch (channelOutputMode)
			{
			case ChannelOutputMode::Stereo:
				for (int n = 0; n < frameSampleCount; n++)
				{
					leftChannelSamples[n] += instLeftChannelSamples[n];
					rightChannelSamples[n] += instRightChannelSamples[n];
				}
				break;

			case ChannelOutputMode::Mono:
				for (int n = 0; n < frameSampleCount; n++)
					leftChannelSamples[n] += instLeftChannelSamples[n] + instRightChannelSamples[n];
				break;
			}
		}
	}

	//应用效果器到乐器的声道buffer
	void Ventrue::ApplyEffectsToChannelBuffer()
	{
		if (isSoundEnd)
			return;

		//对声道应用效果器
		effects->Process();

		if (isVirInstSoundEnd)
		{
			//检测由效果器带来的尾音是否结束
			int offset = (int)(frameSampleCount * 0.02f);
			for (int i = 0; i < frameSampleCount; i += offset)
			{
				if (fabsf(leftChannelSamples[i]) > 0.0001f ||
					fabsf(rightChannelSamples[i]) > 0.0001f)
					return;
			}

			isSoundEnd = true;
		}
	}


	//合并声道buffer到数据流
	void Ventrue::CombineChannelBufferToStream()
	{
		//合并左右声道采样值到流
		float* out = (float*)synthSampleStream;

		switch (channelOutputMode)
		{
		case ChannelOutputMode::Stereo:
			for (int i = 0; i < frameSampleCount; i++)
			{
				*out++ = leftChannelSamples[i];
				*out++ = rightChannelSamples[i];
			}

			/*	for (int i = 0; i < frameSampleCount; i++)
				{
					synthSampleRingBuffer->Write<float>(leftChannelSamples[i]);
					synthSampleRingBuffer->Write<float>(rightChannelSamples[i]);
				}*/
			break;

		case ChannelOutputMode::Mono:
			for (int i = 0; i < frameSampleCount; i++)
				*out++ = leftChannelSamples[i];

			//for (int i = 0; i < frameSampleCount; i++)
			//	synthSampleRingBuffer->Write<float>(leftChannelSamples[i]);
			break;
		}
	}

	// 移除已完成所有区域发声处理(采样处理)的KeySounder      
	void Ventrue::RemoveProcessEndedKeySounder()
	{
		int n = 0;
		for (int i = 0; i < virInstList->size(); i++)
		{
			(*virInstList)[i]->RemoveProcessEndedKeySounder();
			if ((*virInstList)[i]->IsSoundEnd())
				soundEndVirInsts[n++] = (*virInstList)[i];
		}

		if (soundEndVirInstCallBack != nullptr && n > 0)
			soundEndVirInstCallBack(this, soundEndVirInsts, n);
	}


}
