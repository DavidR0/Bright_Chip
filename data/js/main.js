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
        const wakeUP = document.getElementById('wakeUp');
        const remo = document.getElementById('remo');

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

        if(wakeUP){
            wakeUP.addEventListener("click", ()=>{
                wakeup = !wakeup;
                if(wakeup) {
                    connection.send("w1");
                } else {
                    connection.send("w0");
                }
                console.log(`Pressed wakeUp with value ${wakeup}`);
            });
        };
      }
    
    })(window, document, undefined);