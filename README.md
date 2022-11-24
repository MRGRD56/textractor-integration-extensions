# Textractor Extensions

## HttpSender

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
Content-Type: text/plain

Lorem ipsum dolor sit amet
```

###### JSON_TEXT

```http request
POST http://localhost:9650/sentence
Content-Type: application/json

{
  "text": "Lorem ipsum dolor sit amet"
}
```

###### JSON_TEXT_WITH_META

```http request
POST http://localhost:9650/sentence
Content-Type: application/json

{
  "text": "Lorem ipsum dolor sit amet",
  "meta": {
    "isCurrentSelect": true,    // sentenceInfo["current select"]
    "processId": 2898235,       // sentenceInfo["process id"]
    "threadNumber": 12,         // sentenceInfo["text number"]
    "threadName": "Some name"   // sentenceInfo["text name"]
  }
}
```

| Value                   | Explanation |
|-------------------------|-------------|
| `"PLAIN_TEXT"`          |             |
| `"JSON_TEXT"`           |             |
| `"JSON_TEXT_WITH_META"` |             |