/*
 * simpleviewer and streamviewer
 */

char index_simple_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title">ESP32-CAM Simplified View</title>   
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="/style.css">
    <style>
      @media (min-width: 800px) and (orientation:landscape) {
        #content {
          display:flex;
          flex-wrap: nowrap;
          flex-direction: column;
          align-items: flex-start;
        }
      }
    </style>

    <!--script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow/tfjs@1.3.1/dist/tf.min.js"--> 
    <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow/tfjs/dist/tf.min.js"></script>
    <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow-models/coco-ssd@2.1.0"> </script>
    <script src="https:\/\/cdn.roboflow.com/0.2.26/roboflow.js"></script>
    
  </head>

  <body class="loading">
    <body>
    <section class="main">
      <div id="logo">
        <label for="nav-toggle-cb" id="nav-toggle" style="float:left;" title="Settings">&#9776;&nbsp;</label>
        <button id="swap-viewer" style="float:left;" title="Swap to full feature viewer">Full</button>
        <button id="get-still" style="float:left;">Get Still</button>
        <button id="toggle-stream" style="float:left;" class="hidden">Start Stream</button>
        <div id="wait-settings" style="float:left;" class="loader" title="Waiting for camera settings to load"></div>
      </div>
      <div id="content">
        <div class="hidden" id="sidebar">
          <input type="checkbox" id="nav-toggle-cb">
            <nav id="menu" style="width:24em;">
              <div class="input-group hidden" id="lamp-group" title="Flashlight LED.&#013;&#013;Warning:&#013;Built-In lamps can be Very Bright! Avoid looking directly at LED&#013;Can draw a lot of power and may cause visual artifacts, affect WiFi or even brownout the camera on high settings">
                <label for="lamp">Light</label>
                <div class="range-min">Off</div>
                <input type="range" id="lamp" min="0" max="100" value="0" class="action-setting">
                <div class="range-max">Full&#9888;</div>
              </div>
              <div class="input-group" id="framesize-group">
                <label for="framesize">Resolution</label>
                <select id="framesize" class="action-setting">
                  <option value="13">UXGA (1600x1200)</option>
                  <option value="12">SXGA (1280x1024)</option>
                  <option value="11">HD (1280x720)</option>
                  <option value="10">XGA (1024x768)</option>
                  <option value="9">SVGA (800x600)</option>
                  <option value="8">VGA (640x480)</option>
                  <option value="7">HVGA (480x320)</option>
                  <option value="6">CIF (400x296)</option>
                  <option value="5">QVGA (320x240)</option>
                  <option value="3">HQVGA (240x176)</option>
                  <option value="1">QQVGA (160x120)</option>
                  <option value="0">THUMB (96x96)</option>
                </select>
              </div>
              <!-- Hide the next entries, they are present in the body so that we
                  can pass settings to/from them for use in the scripting, not for user setting -->
              <div id="rotate" class="action-setting hidden"></div>
              <div id="cam_name" class="action-setting hidden"></div>
              <div id="stream_url" class="action-setting hidden"></div>
            </nav>
        </div>
        <div id=loading>
        <p> The button for showing camera stream will be enabled once models have been loaded. </p>
        </div>
        <figure>
          <div id="stream-container" class="image-container hidden">
            <div class="close close-rot-none" id="close-stream">×</div>
            <img id="stream" crossOrigin="anonymous" src="" width="800" height="600">
          </div>
        </figure>
      </div>
    </section>
  </body>
</body>

  <script>
  
  document.addEventListener('DOMContentLoaded', function (event) {
    var baseHost = document.location.origin;
    var streamURL = 'Undefined';
    var model;
    var modelA;
    var modelB;

    const settings = document.getElementById('sidebar')
    const waitSettings = document.getElementById('wait-settings')
    const lampGroup = document.getElementById('lamp-group')
    const rotate = document.getElementById('rotate')
    const view = document.getElementById('stream')
    const viewContainer = document.getElementById('stream-container')
    const stillButton = document.getElementById('get-still')
    const streamButton = document.getElementById('toggle-stream')
    const closeButton = document.getElementById('close-stream')
    const swapButton = document.getElementById('swap-viewer')
    const loadingMessage = document.getElementById('loading')
    

    const hide = el => {
      el.classList.add('hidden')
    }
    const show = el => {
      el.classList.remove('hidden')
    }

    const disable = el => {
      el.classList.add('disabled')
      el.disabled = true
    }

    const enable = el => {
      el.classList.remove('disabled')
      el.disabled = false
    }

    const updateValue = (el, value, updateRemote) => {
      updateRemote = updateRemote == null ? true : updateRemote
      let initialValue
      if (el.type === 'checkbox') {
        initialValue = el.checked
        value = !!value
        el.checked = value
      } else {
        initialValue = el.value
        el.value = value
      }

      if (updateRemote && initialValue !== value) {
        updateConfig(el);
      } else if(!updateRemote){
        if(el.id === "lamp"){
          if (value == -1) {
            hide(lampGroup)
          } else {
            show(lampGroup)
          }
        } else if(el.id === "cam_name"){
          window.document.title = value;
          console.log('Name set to: ' + value);
        } else if(el.id === "code_ver"){
          console.log('Firmware Build: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value;
          applyRotation();
        } else if(el.id === "stream_url"){
          streamURL = value;
          streamButton.setAttribute("title", `Start the stream :: {streamURL}`);
          console.log('Stream URL set to:' + value);
        }
      }
    }

    var host = `ws://${window.location.hostname}:100/ws`;
    var websocket;
    
    // Initialization
    //window.addEventListener('load', onLoad);

    function onLoad(event) {
        initWebSocket();
    }

    function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(host);
        websocket.onopen  = onConnect;
        websocket.onclose = onDisconnect;
        websocket.onmessage = onMessage;
    }

    function onConnect(event) {
        console.log('Connection opened');
        //websocket.send("Hello")
    }

    function onDisconnect(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }

    function onMessage(event) {
    //console.log('Received a notification from ${event.origin}');
    console.log(event.data);

    detectObject(0);
    detectObject(1);
   }
    
    var rangeUpdateScheduled = false
    var latestRangeConfig

    function updateRangeConfig (el) {
      latestRangeConfig = el
      if (!rangeUpdateScheduled) {
        rangeUpdateScheduled = true;
        setTimeout(function(){
          rangeUpdateScheduled = false
          updateConfig(latestRangeConfig)
        }, 150);
      }
    }

    function updateConfig (el) {
      let value
      switch (el.type) {
        case 'checkbox':
          value = el.checked ? 1 : 0
          break
        case 'range':
        case 'select-one':
          value = el.value
          break
        case 'button':
        case 'submit':
          value = '1'
          break
        default:
          return
      }

      const query = `${baseHost}/control?var=${el.id}&val=${value}`

      fetch(query)
        .then(response => {
          console.log(`request to ${query} finished, status: ${response.status}`)
        })
    }

    document
      .querySelectorAll('.close')
      .forEach(el => {
        el.onclick = () => {
          hide(el.parentNode)
        }
      })

    var publishable_key = "rf_p5taPrAFTXV36W0EMZLaGGt2sHu1";
    var toLoad = {
        model: "taco-dataset-brnom",
        version: 1
    };
    
    roboflow
    .auth({
        publishable_key: publishable_key
    })
    .load(toLoad)
    .then(function (m) {
        modelB = m;
        //hide(loadingMessage);
        //show(streamButton);
        //resolve();
    });


    function loadModels() {
      cocoSsd.load().then(cocoSsd_Model => {
        modelA = cocoSsd_Model;

        hide(loadingMessage);
        show(streamButton);
      }); 
    }

    // read initial values
    fetch(`${baseHost}/status`)
      .then(function (response) {
        return response.json()
      })
      .then(function (state) {
        document
          .querySelectorAll('.action-setting')
          .forEach(el => {
            updateValue(el, state[el.id], false)
          })
        hide(waitSettings);
        show(settings);
        //ObjectDetect(); 
        loadModels();
       // show(streamButton);
       // startStream();
      })

    // Put some helpful text on the 'Still' button
    stillButton.setAttribute("title", `Capture a still image :: ${baseHost}/capture`);

    const stopStream = () => {
      window.stop();
      streamButton.innerHTML = 'Start Stream';
          streamButton.setAttribute("title", `Start the stream :: ${streamURL}`);
      hide(viewContainer);
    }

    const startStream = () => {
      view.src = streamURL;
      view.scrollIntoView(false);
      streamButton.innerHTML = 'Stop Stream';
      streamButton.setAttribute("title", `Stop the stream`);
      show(viewContainer);
      initWebSocket();
    }

    const applyRotation = () => {
      rot = rotate.value;
      if (rot == -90) {
        viewContainer.style.transform = `rotate(-90deg)  translate(-100%)`;
        closeButton.classList.remove('close-rot-none');
        closeButton.classList.remove('close-rot-right');
        closeButton.classList.add('close-rot-left');
      } else if (rot == 90) {
        viewContainer.style.transform = `rotate(90deg) translate(0, -100%)`;
        closeButton.classList.remove('close-rot-left');
        closeButton.classList.remove('close-rot-none');
        closeButton.classList.add('close-rot-right');
      } else {
        viewContainer.style.transform = `rotate(0deg)`;
        closeButton.classList.remove('close-rot-left');
        closeButton.classList.remove('close-rot-right');
        closeButton.classList.add('close-rot-none');
      }
       console.log('Rotation ' + rot + ' applied');
   }

    // Attach actions to controls

    stillButton.onclick = () => {
      stopStream();
      view.src = `${baseHost}/capture?_cb=${Date.now()}`;
      view.scrollIntoView(false);
      show(viewContainer);
    }

    closeButton.onclick = () => {
      stopStream();
      hide(viewContainer);
    }

    streamButton.onclick = () => {
      const streamEnabled = streamButton.innerHTML === 'Stop Stream'
      if (streamEnabled) {
        stopStream();
      } else {
        startStream();
      }
    }

    // Attach default on change action
    document
      .querySelectorAll('.action-setting')
      .forEach(el => {
        el.onchange = () => updateConfig(el)
      })

    // Update range sliders as they are being moved
    document
      .querySelectorAll('input[type="range"]')
      .forEach(el => {
        el.oninput = () => updateRangeConfig(el)
      })

    // Custom actions
    // Detection and framesize
    rotate.onchange = () => {
      applyRotation();
      updateConfig(rotate);
    }

    framesize.onchange = () => {
      updateConfig(framesize)
    }
    
    swapButton.onclick = () => {
      window.open('/?view=full','_self');
    }

    view.onload = () => {
      detectObject(0);
    }

    var children = [];
    
function detectObject(num) {

let n;
let match_level;

  switch(num) {
    
    case 0:
      model = modelA;
      break;
      
    case 1:
      model = modelB;
      //match_level = 
      break;
  }

  // Now let's start classifying a frame in the stream.
  
  model.detect(view).then(function (predictions, match_level) {
    
    // Remove any highlighting we did previous frame.
    for (let i = 0; i < children.length; i++) {
      viewContainer.removeChild(children[i]);
    }
    children.splice(0);
    
    // Now lets loop through predictions and draw them to the live view if
    // they have a high confidence score.
    for (n = 0; n < predictions.length; n++) {

      match_level = 'score' in predictions[n] ? predictions[n].score : predictions[n].confidence;
 
      // If we are over 66% sure we are sure we classified it right, draw it!
      //if (predictions[n].score > 0.7) {

        //if (predictions[n].class == 'bottle') {
          if (predictions[n].class != 'vase') {

          //console.log('A ' + predictions[n].class + ' was detected');
          //websocket.send("Valid")
          websocket.send(JSON.stringify({'object':predictions[n].class}));
        //}
      //}

        console.log('A ' + predictions[n].class + ' was detected');
         
        const p = document.createElement('p');
        p.innerText = predictions[n].class  + ' - with ' 
           // + Math.round(parseFloat(predictions[n].score) * 100) 
           + Math.round(parseFloat(match_level) * 100) 
            + '% confidence.';
        p.style = 'margin-left: ' + predictions[n].bbox[0] + 'px; margin-top: '
            + (predictions[n].bbox[1] - 10) + 'px; width: ' 
            + (predictions[n].bbox[2] - 10) + 'px; top: 0; left: 0;';

        const highlighter = document.createElement('div');
        highlighter.setAttribute('class', 'highlighter');
        highlighter.style = 'left: ' + predictions[n].bbox[0] + 'px; top: '
            + predictions[n].bbox[1] + 'px; width: ' 
            + predictions[n].bbox[2] + 'px; height: '
            + predictions[n].bbox[3] + 'px;';

        viewContainer.appendChild(highlighter);
        viewContainer.appendChild(p);
        children.push(highlighter);
        children.push(p);

        return;
     // }
     } 
    }
    
      // Call this function again to keep predicting classes when the browser is ready.
      window.requestAnimationFrame(detectObject);
    });
   
  }
})
  
  </script>
</html>)=====";

size_t index_simple_html_len = sizeof(index_simple_html)-1;

/* Stream Viewer */

const uint8_t streamviewer_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title">ESP32-CAM StreamViewer</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <style>
      /* No stylesheet, define all style elements here */
      body {
        font-family: Arial,Helvetica,sans-serif;
        background: #181818;
        color: #EFEFEF;
        font-size: 16px;
        margin: 0px;
        overflow:hidden;
      }

      img {
        object-fit: contain;
        display: block;
        margin: 0px;
        padding: 0px;
        width: 100vw;
        height: 100vh;
      }

      .loader {
        border: 0.5em solid #f3f3f3;
        border-top: 0.5em solid #000000;
        border-radius: 50%;
        width: 1em;
        height: 1em;
        -webkit-animation: spin 2s linear infinite; /* Safari */
        animation: spin 2s linear infinite;
      }

      @-webkit-keyframes spin {   /* Safari */
        0% { -webkit-transform: rotate(0deg); }
        100% { -webkit-transform: rotate(360deg); }
      }

      @keyframes spin {
        0% { transform: rotate(0deg); }
        100% { transform: rotate(360deg); }
      }
    </style>
  </head>

  <body>
    <section class="main">
      <div id="wait-settings" style="float:left;" class="loader" title="Waiting for stream settings to load"></div>
      <div style="display: none;">
        <!-- Hide the next entries, they are present in the body so that we
             can pass settings to/from them for use in the scripting -->
        <div id="rotate" class="action-setting hidden">0</div>
        <div id="cam_name" class="action-setting hidden"></div>
        <div id="stream_url" class="action-setting hidden"></div>
      </div>
      <img id="stream" src="">
    </section>
  </body>

  <script>
  document.addEventListener('DOMContentLoaded', function (event) {
    var baseHost = document.location.origin;
    var streamURL = 'Undefined';

    const rotate = document.getElementById('rotate')
    const stream = document.getElementById('stream')
    const spinner = document.getElementById('wait-settings')

    const updateValue = (el, value, updateRemote) => {
      updateRemote = updateRemote == null ? true : updateRemote
      let initialValue
      if (el.type === 'checkbox') {
        initialValue = el.checked
        value = !!value
        el.checked = value
      } else {
        initialValue = el.value
        el.value = value
      }

      if (updateRemote && initialValue !== value) {
        updateConfig(el);
      } else if(!updateRemote){
        if(el.id === "cam_name"){
          window.document.title = value;
          stream.setAttribute("title", value + "\n(doubleclick for fullscreen)");
          console.log('Name set to: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value;
          console.log('Rotate recieved: ' + rotate.value);
        } else if(el.id === "stream_url"){
          streamURL = value;
          console.log('Stream URL set to:' + value);
        }
      }
    }

    // read initial values
    fetch(`${baseHost}/info`)
      .then(function (response) {
        return response.json()
      })
      .then(function (state) {
        document
          .querySelectorAll('.action-setting')
          .forEach(el => {
            updateValue(el, state[el.id], false)
          })
        spinner.style.display = `none`;
        applyRotation();
        startStream();
      })

    const startStream = () => {
      stream.src = streamURL;
      stream.style.display = `block`;
    }

    const applyRotation = () => {
      rot = rotate.value;
      if (rot == -90) {
        stream.style.transform = `rotate(-90deg)`;
      } else if (rot == 90) {
        stream.style.transform = `rotate(90deg)`;
      }
      console.log('Rotation ' + rot + ' applied');
    }

    stream.ondblclick = () => {
      if (stream.requestFullscreen) {
        stream.requestFullscreen();
      } else if (stream.mozRequestFullScreen) { /* Firefox */
        stream.mozRequestFullScreen();
      } else if (stream.webkitRequestFullscreen) { /* Chrome, Safari and Opera */
        stream.webkitRequestFullscreen();
      } else if (stream.msRequestFullscreen) { /* IE/Edge */
        stream.msRequestFullscreen();
      }
    }
  })
  </script>
</html>)=====";

size_t streamviewer_html_len = sizeof(streamviewer_html)-1;

/* Captive Portal page
   we replace the <> delimited strings with correct values as it is served */

const std::string portal_html = R"=====(<!doctype html>
<!DOCTYPE html>
<html>
<head>
  <title>RVM Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="stylesheet" type="text/css" href="/wmStyle.css">
</head>
<body>
  <div class="topnav">
    <h1>Reverse Vendo Wi-Fi Manager</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br>
            <label for="ip">IP Address</label>
            <input type="text" id ="ip" name="ip" value="192.168.1.30"><br>
            <label for="gateway">Gateway Address</label>
            <input type="text" id ="gateway" name="gateway" value="192.168.1.1"><br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>)=====";

/* Error page
   we replace the <> delimited strings with correct values as it is served */

const std::string error_html = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title"><CAMNAME> - Error</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="ico\" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="<APPURL>style.css">
  </head>
  <body style="text-align: center;">
    <img src="<APPURL>logo.svg" style="position: relative; float: right;">
    <h1><CAMNAME></h1>
    <ERRORTEXT>
  </body>
  <script>
    setTimeout(function(){
      location.replace(document.URL);
    }, 60000);
  </script>
</html>)=====";