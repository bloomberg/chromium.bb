import {$} from 'chrome://resources/js/util.m.js';

import {AudioBroker} from './audio_broker.js';
import {AudioPlayer} from './audio_player.js';
import {Page} from './page.js';



export interface AudioSample {
  sampleRate: number;
  freqency: number;
  channelCount: 1|2;
  pan: number;
  description: string;
}

let audiosSamples: AudioSample[] = [
  {
    sampleRate: 44100,
    freqency: 440,
    channelCount: 1,
    pan: 0,
    description: '44.1k mono 440Hz sine tone'
  },
  {
    sampleRate: 48000,
    freqency: 440,
    channelCount: 1,
    pan: 0,
    description: '48k mono 440Hz sine tone'
  },
  {
    sampleRate: 48000,
    freqency: 440,
    channelCount: 2,
    pan: 0,
    description: '48k stereo 440Hz sine tone'
  },
  {
    sampleRate: 48000,
    freqency: 440,
    channelCount: 2,
    pan: -1,
    description: '48k stereo 440Hz sine tone - Left channel only'
  },
  {
    sampleRate: 48000,
    freqency: 440,
    channelCount: 2,
    pan: 1,
    description: '48k stereo 440Hz sine tone - Right channel only'
  },
];

export class OutputPage extends Page {
  testOutputFeedback: Map<string, null|boolean>;
  constructor() {
    super('output');
    this.testOutputFeedback = new Map();
    this.initOutputMap();
    this.createAudioPlayer();
  }

  initOutputMap() {
    for (const audio of audiosSamples) {
      this.testOutputFeedback.set(audio.description, null);
    }
  }

  updateActiveOutputDevice() {
    const handler = AudioBroker.getInstance().handler;
    handler.getActiveOutputDeviceName().then(({deviceName}) => {
      if (deviceName) {
        $('active-output').innerHTML = deviceName;
      } else {
        $('active-output').innerHTML = 'No active output device';
      }
    });
  }

  setOutputMapEntry(audioSample: AudioSample, canHear: boolean) {
    this.testOutputFeedback.set(audioSample.description, canHear);
    console.log(this.testOutputFeedback);
  }

  createAudioPlayer() {
    const audioPlayer = new AudioPlayer(audiosSamples);
    $('audio-player').appendChild(audioPlayer);
  }

  static getInstance() {
    if (instance === null) {
      instance = new OutputPage();
    }
    return instance;
  }
}

let instance: OutputPage|null = null;
