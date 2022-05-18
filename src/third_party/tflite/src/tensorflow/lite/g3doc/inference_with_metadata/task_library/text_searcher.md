# Integrate text searchers

Text search allows searching for semantically similar text in a corpus. It works
by embedding the search query into a high-dimensional vector representing the
semantic meaning of the query, followed by similarity search in a predefined,
custom index using
[ScaNN](https://github.com/google-research/google-research/tree/master/scann)
(Scalable Nearest Neighbors).

As opposed to text classification (e.g.
[Bert natural language classifier](https://www.tensorflow.org/lite/inference_with_metadata/task_library/bert_nl_classifier)),
expanding the number of items that can be recognized doesn't require re-training
the entire model. New items can be added simply re-building the index. This also
enables working with larger (100k+ items) corpuses.

Use the Task Library `TextSearcher` API to deploy your custom text searcher into
your mobile apps.

## Key features of the TextSearcher API

*   Takes a single string as input, performs embedding extraction and
    nearest-neighbor search in the index.

*   Input text processing, including in-graph or out-of-graph
    [Wordpiece](https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h)
    or
    [Sentencepiece](https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/cc/text/tokenizers/sentencepiece_tokenizer.h)
    tokenizations on input text.

## Prerequisites

Before using the `TextSearcher` API, an index needs to be built based on the
custom corpus of text to search into. This can be achieved using
[Model Maker](https://www.tensorflow.org/lite/guide/model_maker).

For this you will need:

*   a TFLite text embedder model such as the
    [universal sentence encoder](https://tfhub.dev/google/lite-model/universal-sentence-encoder-qa-ondevice/1)
    model,
*   your corpus of text.

After this step, you should have a standalone TFLite searcher model (e.g.
`mobilenet_v3_searcher.tflite`), which is the original text embedder model with
the index attached into the
[TFLite Model Metadata](https://www.tensorflow.org/lite/convert/metadata).

## Run inference in Java

### Step 1: Import Gradle dependency and other settings

Copy the `.tflite` searcher model file to the assets directory of the Android
module where the model will be run. Specify that the file should not be
compressed, and add the TensorFlow Lite library to the module’s `build.gradle`
file:

```java
android {
    // Other settings

    // Specify tflite index file should not be compressed for the app apk
    aaptOptions {
        noCompress "tflite"
    }

}

dependencies {
    // Other dependencies

    // Import the Task Vision Library dependency (NNAPI is included)
    implementation 'org.tensorflow:tensorflow-lite-task-vision:0.4.0'
    // Import the GPU delegate plugin Library for GPU inference
    implementation 'org.tensorflow:tensorflow-lite-gpu-delegate-plugin:0.4.0'
}
```

### Step 2: Using the model

```java
// Initialization
TextSearcherOptions options =
    TextSearcherOptions.builder()
        .setBaseOptions(BaseOptions.builder().useGpu().build())
        .setSearcherOptions(
            SearcherOptions.builder().setL2Normalize(true).build())
        .build();
TextSearcher textSearcher =
    textSearcher.createFromFileAndOptions(context, modelFile, options);

// Run inference
List<NearestNeighbor> results = textSearcher.search(text);
```

See the
[source code and javadoc](https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/searcher/TextSearcher.java)
for more options to configure the `TextSearcher`.

## Run inference in C++

```c++
// Initialization
TextSearcherOptions options;
options.mutable_base_options()->mutable_model_file()->set_file_name(model_file);
options.mutable_embedding_options()->set_l2_normalize(true);
std::unique_ptr<TextSearcher> text_searcher = TextSearcher::CreateFromOptions(options).value();

// Run inference
const SearchResult result = text_searcher->Search(text).value();
```

See the
[source code](https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/cc/task/text/text_searcher.h)
for more options to configure `TextSearcher`.

## Run inference in Python

### Step 1: Install TensorFlow Lite Support Pypi package.

You can install the TensorFlow Lite Support Pypi package using the following
command:

```sh
pip install tflite-support
```

### Step 2: Using the model

```python
from tflite_support.task import text

# Initialization
text_searcher = text.TextSearcher.create_from_file(model_file)

# Run inference
result = text_searcher.search(text)
```

See the
[source code](https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/python/task/text/text_searcher.py)
for more options to configure `TextSearcher`.

## Example results

```
Results:
 Rank#0:
  metadata: The sun was shining on that day.
  distance: 0.04618
 Rank#1:
  metadata: It was a sunny day.
  distance: 0.10856
 Rank#2:
  metadata: The weather was excellent.
  distance: 0.15223
 Rank#3:
  metadata: The cat is chasing after the mouse.
  distance: 0.34271
 Rank#4:
  metadata: He was very happy with his newly bought car.
  distance: 0.37703
```

Try out the simple
[CLI demo tool for TextSearcher](https://github.com/tensorflow/tflite-support/tree/master/tensorflow_lite_support/examples/task/text/desktop#textsearcher)
with your own model and test data.
