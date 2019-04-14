// Create socket
var socket = io.connect("http://" + document.domain + ":" + location.port);
var time = 0;

const getRate = 250;
const refreshRate = 5000;

window.setInterval(function () {
    socket.emit("getStatus");
}, getRate);

window.setInterval(function () {
    socket.emit("refreshStatus");
}, refreshRate);

$(document).ready(function () {
    $(".toggleBtn").click(function () {
        var tID = this.id;

        if ($(this).val() === "OFF") {
            socket.emit("setStatus", [{ name: tID, value: true }]);
        }
        else if ($(this).val() === "ON") {
            socket.emit("setStatus", [{ name: tID, value: false }]);
        }
        else if ($(this).val() === "CLOSED") {
            socket.emit("setStatus", [{ name: tID, value: true }]);
        }
        else if ($(this).val() === "OPEN") {
            socket.emit("setStatus", [{ name: tID, value: false }]);
        }
    });
});

socket.on("updateStatus", function (msg) {
    var objs;
    if (typeof msg !== "string") {
        objs = JSON.parse(JSON.stringify(msg));
    }
    else {
        objs = JSON.parse(msg);
    }
    console.log(objs);
    for (var i = 0; i < objs.length; ++i) {
        var itm = document.getElementById(objs[i].name);
        console.log("tag: " + itm.tagName);
        switch (itm.tagName) {
            case "H3":
                var t = objs[i].value.toString();
                itm.nextElementSibling.innerHTML = t;
                break;
            case "INPUT":
                console.log("type: " + itm.type);
                if (itm.type === "button") {
                    if ($(itm).hasClass("t_on_off")) {
                        if (objs[i].value == true) {
                            $(itm).val("ON");
                            $(itm).addClass("toggleBtn_ON").removeClass("toggleBtn_OFF");
                        }
                        else {
                            $(itm).val("OFF");
                            $(itm).addClass("toggleBtn_OFF").removeClass("toggleBtn_ON");
                        }
                    }
                    else if ($(itm).hasClass("t_open_closed")) {
                        if (objs[i].value == true) {
                            $(itm).val("OPEN");
                            $(itm).addClass("toggleBtn_ON").removeClass("toggleBtn_OFF");
                        }
                        else {
                            $(itm).val("CLOSED");
                            $(itm).addClass("toggleBtn_OFF").removeClass("toggleBtn_ON");
                        }
                    }
                }
                break;
        }
    }
});
