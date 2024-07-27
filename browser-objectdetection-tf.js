const view = document.getElementById('stream')
const viewContainer = document.getElementById('stream-container')

var host = `ws://${window.location.hostname}:100/ws`;
var websocket;

window.addEventListener('load', onLoad);

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

websocket = new WebSocket(host);
websocket.onopen  = onConnect;
websocket.onclose = onDisconnect;
websocket.onmessage = onMessage;

function onConnect(event) {
        console.log('Connection opened');
        websocket.send("Hello")
    }

function onDisconnect(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }

function onMessage(event) {
    console.log(event.data);
   }


function loadModels() {
      cocoSsd.load().then(cocoSsd_Model => {
        Model = cocoSsd_Model;
        hide(loadingMessage);
        show(streamButton);
      }); 
    }


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
        loadModels();
      })


view.onload = () => {
      detectObject();
    }

var children = [];


function detectObject() {
    
  Model.detect(view).then(function (predictions) {
    // Remove any highlighting we did in the previous frame
    for (let i = 0; i < children.length; i++) {
      viewContainer.removeChild(children[i]);
    }
    children.splice(0);
    
    // Loop through predictions and draw them stream view if they have a high confidence score

    for (let n = 0; n < predictions.length; n++) {
        
      if (predictions[n].score > 0.66) {

        if (predictions[n].class == 'bottle') {
          console.log('A ' + predictions[n].class + ' was detected');
          //websocket.send("Valid")
          websocket.send(JSON.stringify({'object':predictions[n].class}));
        }
          
        const p = document.createElement('p');
        p.innerText = predictions[n].class  + ' - with ' 
            + Math.round(parseFloat(predictions[n].score) * 100) 
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
      }
    }
    
      // Call this function again to keep predicting classes when the browser is ready
      window.requestAnimationFrame(detectObject);
    });
   }
  
