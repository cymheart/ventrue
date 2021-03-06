﻿#ifndef _MidiTrack_h_
#define _MidiTrack_h_

#include"MidiTypes.h"


namespace ventrue
{
	class  MidiTrack
	{
	public:
		MidiTrack();
		MidiTrack(const MidiTrack& obj);

		~MidiTrack();

		//设置一个四分音符所要弹奏的tick数
		inline void SetTickForQuarterNote(float tickForQuarterNote)
		{
			this->tickForQuarterNote = tickForQuarterNote;
		}

		/// <summary>
		/// 增加一个事件
		/// </summary>
		/// <param name="ev"></param>
		void AddEvent(MidiEvent* ev);

		void AddNoteOnEventToMap(int note, int channel, NoteOnEvent* midiEvent);

		/// <summary>
		/// 获取事件列表
		/// </summary>
		/// <returns></returns>
		inline list<MidiEvent*>* GetEventList()
		{
			return &midiEventList;
		}

		/// <summary>
		/// 获取通道分类的事件列表
		/// </summary>
		/// <returns></returns>
		inline MidiEventList* GetEventListAtChannel()
		{
			return midiEventListAtChannel;
		}

		/// <summary>
		/// 获取事件数量
		/// </summary>
		/// <returns></returns>
		inline int GetEventCount()
		{
			return (int)midiEventList.size();
		}

		/// <summary>
		/// 寻找匹配的NoteOnEvent
		/// </summary>
		/// <param name="note"></param>
		/// <param name="channel"></param>
		/// <returns></returns>
		NoteOnEvent* FindNoteOnEvent(int note, int channel);

		//改变轨道所有Midi事件中一个四分音符所要弹奏的tick数
		void ChangeMidiEventsTickForQuarterNote(float changedTickForQuarterNote);

	private:
		list<MidiEvent*> midiEventList;
		MidiEventList midiEventListAtChannel[16];

		//一个四分音符所要弹奏的tick数,默认值120
		float tickForQuarterNote = 120;

		unordered_map<int, vector<NoteOnEvent*>> noteOnEventMap;

	};

}

#endif
