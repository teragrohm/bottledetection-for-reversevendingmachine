In order to import the TensorFlow.js library that enables Machine Learning in web applications, this line of code will be included in the header file of the HTML for the webpage where you can view the camera footage.


```python
<script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow/tfjs@1.3.1/dist/tf.min.js"> </script>
```

This next line loads the pre-trained models from the COCO dataset wherein **bottle** is one of the objects that can be detected from an image or from a frame.


```python
 <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow-models/coco-ssd@2.1.0"> </script>
```



A variable with the name *host* will be declared and the value assigned to it will allow connection to the **WebSocket** server hosted on the ESP32-CAM board which the ESP32 will later be used to transfer the result of the image processing from the web application to the ESP32CAM.


```python
var host = `ws://${window.location.hostname}:100/ws`;
```

The line below enclosed in brackets is a placeholder automatically replaced by the IP address of the ESP32-CAM that is hosting the camera stream webpage. **100** is the port where it listens for new connections via WebSocket.


```python
${window.location.hostname}:100
```

Declare a variable with the name *websocket* which will later store a value later.


```python
var websocket;
```

In the line below, the **addEventListener()** method will be applied to the window or webpage that loads when the camera stream is opened on a browser. The arguments given to this method mean that the window listens for the *load* event or for the webpage to load, and then a callback function **onLoad()** will be executed.


```python
 window.addEventListener('load', onLoad);
```

The **onLoad** callback will execute the **initWebSocket()** function.


```python
function onLoad(event) {
        initWebSocket();
    }
```

In the block of code below, the **initWebSocket()** function is defined. The statement logged into the console is mainly for testing during development to give the developer an indication that the function is now being called.


```python
function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(host);
        websocket.onopen  = onConnect;
        websocket.onclose = onDisconnect;
        websocket.onmessage = onMessage;
    }
```

A **WebSocket oject** is initialized to create a **WebSocket client** that can be addressed using the websocket variable. The argument *host* supplied to object means that this WebSocket client will attempt to the **WebSocket** host declared earlier in the code.


```python
 websocket = new WebSocket(host);
```

Callback functions will be assigned to the *onopen*, *onclose*, and *onmessage* methods of the websocket object.


```python
websocket.onopen  = onConnect;
websocket.onclose = onDisconnect;
websocket.onmessage = onMessage;
```

Inside the definition of the onConnect callback function, a message will be logged into the console to signal that a connection to the WebSocket server has been opened. The JavaScript program will proceed with sending a "Hello" message to the WebSocket server hosted in the ESP32-CAM.


```python
function onConnect(event) {
        console.log('Connection opened');
        websocket.send("Hello")
    }
```

Likewise, a statement will be printed on the console after a 2 second delay if the connection to the WebSocket server closes.


```python
function onDisconnect(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }
```

Whenever the WebSocket server sends message to the websocket client object in this script and the onMessage callback function is executed, the data will be printed on the console.


```python
function onMessage(event) {
    console.log(event.data);
   }
```



A **loadModels()** function will be defined to disable the display for the camera footage if the COCO models have not been loaded yet.


```python
function loadModels() {
      cocoSsd.load().then(cocoSsd_Model => {
        Model = cocoSsd_Model;
        hide(loadingMessage);
        show(streamButton);
      }); 
    }
```

Once the cocoSsd_Model object to be used with TensorFlow.js for the image processing has finished being loaded, this will be stored in the Model variable declared earlier.


```python
 Model = cocoSsd_Model;
```

The loading message below will disappear when the COCO machine learning or object detection models have been loaded.

Insert image

The Start stream button below will also appear.


