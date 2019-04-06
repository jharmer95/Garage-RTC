// Create socket
var socket = io.connect("http://" + document.domain + ":" + location.port);
var time = 0;

const refreshRate = 250;

window.setInterval(function() {
    socket.emit("pull pins");
}, refreshRate);

$(document).ready(function () {
    $(".toggleBtn").click(function () {
        var tID = this.id;

        if ($(this).val() === "OFF") {
            //console.log("OFF");
            socket.emit("toggle", {id: tID, val: true});
        }
        else if  ($(this).val() === "ON") {
            //console.log("ON");
            socket.emit("toggle", {id: tID, val: false});
        }
    });
});

socket.on("update pins", function (msg) {
    var objs;
    if (typeof msg !== "string") {
        objs = JSON.parse(JSON.stringify(msg));
    }
    else {
        objs = JSON.parse(msg);
    }

    for (var i = 0; i < objs.length; ++i) {
        var idVal = "#" + objs[i].id;

        if (objs[i].val == true) {
            $(idVal).val("ON");
            $(idVal).addClass("toggleBtn_ON").removeClass("toggleBtn_OFF");
        }
        else {
            $(idVal).val("OFF");
            $(idVal).addClass("toggleBtn_OFF").removeClass("toggleBtn_ON");
        }
    }
});
