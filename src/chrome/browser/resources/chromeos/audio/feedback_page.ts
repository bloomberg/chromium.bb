import {$} from 'chrome://resources/js/util.m.js';

import {AudioBroker} from './audio_broker.js';
import {InputPage} from './input_page.js';
import {OutputPage} from './output_page.js';
import {Page} from './page.js';

interface feedbackObject {
  [key: string]: any
}

export class FeedbackPage extends Page {
  private audioInfoString: string = '';
  private inputFeedbackMap: Map<string, null|string>;
  private outputFeedbackMap: Map<string, null|boolean>;
  constructor() {
    super('feedback');
    this.inputFeedbackMap = InputPage.getInstance().testInputFeedback;
    this.outputFeedbackMap = OutputPage.getInstance().testOutputFeedback;
    this.registerButtons();
  }

  showPage() {
    super.showPage();
    this.updateAudioInfo();
    this.updateDownloadButton();
  }

  registerButtons() {
    $('copy-btn').addEventListener('click', () => {
      navigator.clipboard.writeText(this.audioInfoString);
    });
    $('submit-btn').addEventListener('click', () => {
      AudioBroker.getInstance().handler.openFeedbackDialog();
    });
  }

  updateDownloadButton() {
    if (this.inputFeedbackMap.has('audioUrl')) {
      const url = this.inputFeedbackMap.get('audioUrl');
      if (url) {
        const downloadBtn = <HTMLAnchorElement>$('download-btn');
        const inputAudio = <HTMLAudioElement>$('test-input-audio');
        inputAudio.src = url;
        downloadBtn.href = url;
        downloadBtn.download =
            'test_input_' + new Date().toISOString() + '.wav';
        $('input-replay').hidden = false;
      }
    }
  }

  updateAudioInfo() {
    var audioInfoJson: feedbackObject = {};

    const inputFeedbackObject = this.mapToObject(this.inputFeedbackMap);
    const outputFeedbackObject = this.mapToObject(this.outputFeedbackMap);

    audioInfoJson['inputFeedback'] = inputFeedbackObject;
    audioInfoJson['outputFeedback'] = outputFeedbackObject;
    const infoString = JSON.stringify(audioInfoJson);
    const guidedQuestions = `#cros-audio \n
    1. What is the app/website that you are having
    audio issue with (please give url or app name): \n
    2. Describe your audio device in detail: \n
    3. Any specific behavior you notice during the testing process?: \n
    4. audio info: `;
    this.audioInfoString = guidedQuestions + infoString;
    (<HTMLTextAreaElement>$('audio-info')).value = this.audioInfoString;
  }

  mapToObject(map: Map<string, any>) {
    const tempObject: feedbackObject = {};
    map.forEach((value: any, key: string) => {
      tempObject[key] = value;
    });
    console.log(tempObject);
    return tempObject;
  }

  static getInstance() {
    if (instance === null) {
      instance = new FeedbackPage();
    }
    return instance;
  }
}

let instance: FeedbackPage|null = null;