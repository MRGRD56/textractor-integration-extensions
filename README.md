# Textractor Extensions

## TextractorPipe

The most stable extension here.  
Creates a [named pipe](https://en.wikipedia.org/wiki/Named_pipe) and sends sentence data to it.

The pipe name is `\\.\pipe\MRGRD56_TextractorPipe_f30799d5-c7eb-48e2-b723-bd6314a03ba2`.

This extension is currently used by the latest version of [Textractor Translator](https://github.com/MRGRD56/textractor-translator).

The data is sent in JSON format:

```json
{
  "text": "Lorem ipsum dolor sit amet",
  "meta": {
    "isCurrentSelect": true,    // sentenceInfo["current select"]
    "processId": 2898235,       // sentenceInfo["process id"]
    "threadNumber": 12,         // sentenceInfo["text number"]
    "threadName": "Some name",  // sentenceInfo["text name"]
    "timestamp": 1669721578
  }
}
```

Every message in the pipe starts from the length of data that is represented by an `unsigned int32` that is exactly 4 bytes long (it is not text data!). The size is followed by JSON text data, whose length in bytes is equal to the number before it.

| Range                 | Description                                                        |
|-----------------------|--------------------------------------------------------------------|
| [0..3]                | `uint32` representing the length of the message                    |
| [4..{length + 4 - 1}] | JSON `string` representing the message                             |

## HttpSender

Asynchronously sends each sentence as an HTTP request. Can be used to integrate Textractor with other applications.

### Configuration

#### HttpSenderConfig.json

##### Example
```json
{
  "sentence": {
    "enabled": false,
    "url": "http://localhost:9650/sentence",
    "requestType": "PLAIN_TEXT",
    "selectedThreadOnly": true
  }
}
```

| Field                | Type                                                               |
|----------------------|--------------------------------------------------------------------|
| `enabled`*           | boolean                                                            |
| `url`                | string                                                             |
| `requestType`**      | `"PLAIN_TEXT"` &vert; `"JSON_TEXT"` &vert; `"JSON_TEXT_WITH_META"` |
| `selectedThreadOnly` | boolean                                                            |

**⚠️⚠️⚠️&ast;Set `enabled` field to `true` to make it work**

##### **`requestType` field

###### PLAIN_TEXT

```http request
POST http://localhost:9650/sentence
Content-Type: text/plain; charset=UTF-8

Lorem ipsum dolor sit amet
```

###### JSON_TEXT

```http request
POST http://localhost:9650/sentence
Content-Type: application/json; charset=UTF-8

{
  "text": "Lorem ipsum dolor sit amet"
}
```

###### JSON_TEXT_WITH_META

```http request
POST http://localhost:9650/sentence
Content-Type: application/json; charset=UTF-8

{
  "text": "Lorem ipsum dolor sit amet",
  "meta": {
    "isCurrentSelect": true,    // sentenceInfo["current select"]
    "processId": 2898235,       // sentenceInfo["process id"]
    "threadNumber": 12,         // sentenceInfo["text number"]
    "threadName": "Some name",  // sentenceInfo["text name"]
    "timestamp": 1669721578
  }
}
```

## TextractorTranslatorBridge

Zero config version of `HttpSender` made specially for some old versions of [Textractor Translator](https://github.com/MRGRD56/textractor-translator)

Matches the following config of `HttpSender`:

```json
{
  "sentence": {
    "enabled": true,
    "method": "POST",
    "url": "http://localhost:18952/sentence",
    "requestType": "JSON_TEXT_WITH_META",
    "selectedThreadOnly": true
  }
}
```
