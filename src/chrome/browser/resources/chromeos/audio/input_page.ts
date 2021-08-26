import {$} from 'chrome://resources/js/util.m.js';

import {AudioBroker} from './audio_broker.js';
import {Page, PageNavigator} from './page.js';


export class InputPage extends Page {
  testInputFeedback: Map<string, null|string>;
  private analyserLeft?: AnalyserNode;
  private analyserRight?: AnalyserNode;
  private animationRequestId?: number;
  private recordClicked: boolean;
  private audioContext: AudioContext|null;
  // Type is set to any because TypeScript compiler
  // does not recognize the MediaRecorder object.
  private mediaRecorder: any;
  private intervalId: number|null;

  constructor() {
    super('input');
    this.audioContext = null;
    this.mediaRecorder = null;
    this.recordClicked = false;
    this.intervalId = null;
    this.testInputFeedback =
        new Map([['audioUrl', null], ['Can Hear Clearly', null]]);
    this.setUpButtons();
  }

  showPage() {
    super.showPage();
    if (this.audioContext) {
      this.audioContext.resume();
    } else {
      this.initAudio(true);
    }
  }

  hidePage() {
    super.hidePage();
    if (this.audioContext) {
      this.audioContext.suspend();
    }
    if (this.recordClicked) {
      this.stopRecord();
    }
  }

  updateActiveInputDevice() {
    const handler = AudioBroker.getInstance().handler;
    handler.getActiveInputDeviceName().then(({deviceName}) => {
      $('active-input').innerHTML = deviceName;
    });
  }

  visualize() {
    const pairs = [
      {'canvas': $('channel-l'), 'analyser': this.analyserLeft},
      {'canvas': $('channel-r'), 'analyser': this.analyserRight},
    ];
    const draw = () => {
      this.animationRequestId = requestAnimationFrame(draw);
      for (const channel of pairs) {
        let canvas = <HTMLCanvasElement>channel['canvas'];
        let canvasContext = canvas.getContext('2d');
        let analyser = channel['analyser'];

        if (canvasContext && analyser) {
          analyser.fftSize = 2048;
          const bufferSize = analyser.frequencyBinCount;
          const buffer = new Uint8Array(bufferSize);

          /* Since we are using percentage for width and height, make it the
           * real value */
          canvas.width = canvas.clientWidth;
          canvas.height = canvas.clientHeight;

          const width = canvas.clientWidth;
          const height = canvas.clientHeight;

          analyser.getByteTimeDomainData(buffer);
          canvasContext.fillStyle = 'rgb(200, 200, 200)';
          canvasContext.fillRect(0, 0, width, height);
          canvasContext.lineWidth = 2;
          canvasContext.strokeStyle = 'rgb(0, 0, 0)';

          canvasContext.beginPath();

          var dx = width * 1.0 / bufferSize;
          var x = 0;

          for (var i = 0; i < bufferSize; i++) {
            const data = buffer[i];
            if (data) {
              var v = data / 128.0;
              var y = v * height / 2;
              if (i === 0) {
                canvasContext.moveTo(x, y);
              } else {
                canvasContext.lineTo(x, y);
              }
              x += dx;
            }
          }
          canvasContext.lineTo(width, height / 2);
          canvasContext.stroke();
        }
      }
    };
    if (this.animationRequestId)
      window.cancelAnimationFrame(this.animationRequestId);
    draw();
  }

  buildAudioGraph(source: MediaStreamAudioSourceNode) {
    if (this.audioContext) {
      const splitter = this.audioContext.createChannelSplitter(2);
      source.connect(splitter);
      this.analyserLeft = this.audioContext.createAnalyser();
      this.analyserRight = this.audioContext.createAnalyser();
      splitter.connect(this.analyserLeft, 0);
      splitter.connect(this.analyserRight, 1);
    }
  }

  initAudio(audio_constraint: boolean|Object) {
    this.audioContext = new window.AudioContext();
    navigator.mediaDevices.getUserMedia({'audio': audio_constraint})
        .then((stream_got) => {
          if (this.audioContext) {
            const stream = stream_got;
            const source = this.audioContext.createMediaStreamSource(stream);
            this.record(stream);
            this.buildAudioGraph(source);
            this.visualize();
          }
        });
  }

  record(source: MediaStream) {
    let chunks = new Array<Blob>();
    const recordButton = $('record-btn');
    const clipSection = $('audio-file');
    this.mediaRecorder = new MediaRecorder(source);

    recordButton.onclick = () => {
      if (!this.recordClicked) {
        this.startRecord();
      } else {
        this.stopRecord();
      }
    };
    if (this.mediaRecorder) {
      this.mediaRecorder.onstop = () => {
        console.log('data available after MediaRecorder.stop() called.');

        const clipContainer = document.createElement('article');
        const audio = document.createElement('audio');

        audio.setAttribute('controls', '');
        clipContainer.appendChild(audio);
        clipSection.appendChild(clipContainer);

        audio.controls = true;
        const blob = new Blob(chunks, {'type': 'audio/ogg; codecs=opus'});
        chunks = new Array<Blob>();
        const audioURL = window.URL.createObjectURL(blob);
        audio.src = audioURL;
        this.testInputFeedback.set('audioUrl', audioURL);
        console.log('recorder stopped');
      };

      this.mediaRecorder.ondataavailable = (event: dataavailable) => {
        chunks.push(event.data);
      };
    }
  }

  startRecord() {
    if (this.mediaRecorder) {
      const recordButton = $('record-btn');
      const clipSection = $('audio-file');
      this.recordClicked = true;
      this.mediaRecorder.start();
      console.log(this.mediaRecorder.state);
      console.log('recorder started');
      this.startTimer();
      recordButton.className = 'on-stop';
      recordButton.textContent = 'Stop';
      if (clipSection.firstChild) {
        clipSection.removeChild(clipSection.firstChild);
      }
    }
  }

  stopRecord() {
    if (this.mediaRecorder) {
      const recordButton = $('record-btn');
      this.recordClicked = false;
      this.mediaRecorder.stop();
      this.stopTimer();
      console.log(this.mediaRecorder.state);
      console.log('recorder stopped');
      recordButton.className = 'on-record';
      recordButton.textContent = 'Record';
      $('input-qs').hidden = false;
    }
  }

  startTimer() {
    var startTime = Date.now();
    this.intervalId = setInterval(() => {
      var delta = Date.now() - startTime;
      $('counter').innerHTML =
          String(Math.floor(delta / 1000)) + ':' + String(delta % 1000);
    });
  }

  stopTimer() {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      $('counter').innerHTML = '';
    }
  }

  setUpButtons() {
    $('input-yes').addEventListener('click', () => {
      this.testInputFeedback.set('Can Hear Clearly', 'true');
      PageNavigator.getInstance().showPage('feedback');
    });
    $('input-no').addEventListener('click', () => {
      this.testInputFeedback.set('Can Hear Clearly', 'false');
      PageNavigator.getInstance().showPage('feedback');
    });
  }


  static getInstance() {
    if (instance === null) {
      instance = new InputPage();
    }
    return instance;
  }
}

let instance: InputPage|null = null;
declare let MediaRecorder: any;
type dataavailable = any;
