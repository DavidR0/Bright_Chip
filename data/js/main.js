(function(window, document, undefined){

    // code that should be taken care of right away
    let connection = new WebSocket('ws://' + location.hostname + ':82/', ['arduino']);
    let wakeup = false;
    
    window.onload = init;
    
      function init(){
        // the code to be called when the dom has loaded
        // #document has its nodes
        const on = document.getElementById('on');
        const off = document.getElementById('off');
        const WebSocketReset = document.getElementById("websocketReset");

        connection.onopen = function () {
        connection.send('Connect ' + new Date());
        };
        connection.onerror = function (error) {
        console.log('WebSocket Error ', error);
        };
        connection.onmessage = function (e) {
        console.log('Server: ', e.data);
        };
        connection.onclose = function () {
        console.log('WebSocket connection closed');
        };

        if(on){
            on.addEventListener("click", ()=>{
                connection.send("SetLigh1");
                console.log("Pressed on");
            });
        };
        if(off){
            off.addEventListener("click", ()=>{
                connection.send("SetLigh0");
                console.log("Pressed off");
            });
        };
      }
    
    })(window, document, undefined);