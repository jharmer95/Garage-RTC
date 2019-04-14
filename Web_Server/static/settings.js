// Create socket
var socket = io.connect("http://" + document.domain + ":" + location.port);
var time = 0;

$(document).ready(function () {
    // $("#scanBtn").click(function () {
    //     alert("SCANNING!");
    // });
    socket.emit("getSettings");
});

function submitForm() {
    var ipForm = document.getElementById("udpIP");
    var ip = ipForm.value;

    var sIP = ip.split(".");
    var isValid = sIP.length == 4;

    if (isValid) {
        for (var i = 0; i < 4; ++i) {
            var nIP = parseInt(sIP[i]);
            if (nIP > 255 || nIP < 0) {
                isValid = false;
                break;
            }
        }
    }

    if (isValid) {
        socket.emit("setSettings", [{"name" : "udpIP", "value": ip}]);
        $(cycleTimeForm).addClass("validText").removeClass("invalidText");
        document.getElementById("errorMsg").innerHTML = "";
    }
    else {
        $(ipForm).addClass("invalidText").removeClass("validText");
        document.getElementById("errorMsg").innerHTML = "Please enter a valid IPv4 address!";
    }

    var portForm = document.getElementById("udpPort");
    var port = parseInt(portForm.value);

    if (port > 0 && port < 65536) {
        socket.emit("setSettings", [{"name" : "udpPort", "value": port}]);
        $(cycleTimeForm).addClass("validText").removeClass("invalidText");
        document.getElementById("errorMsg").innerHTML = "";
    }
    else {
        $(ipForm).addClass("invalidText").removeClass("validText");
        document.getElementById("errorMsg").innerHTML = "Please enter a valid port!";
    }

    var cycleTimeForm = document.getElementById("screenCycle");
    var cycleTime = cycleTimeForm.value;

    if (!isNaN(cycleTime) && cycleTime.indexOf('.') == -1) {
        socket.emit("setSettings", [{"name" : "screenCycle", "value": cycleTime}]);
        $(cycleTimeForm).addClass("validText").removeClass("invalidText");
        document.getElementById("errorMsg").innerHTML = "";
    }
    else {
        $(cycleTimeForm).addClass("invalidText").removeClass("validText");
        document.getElementById("errorMsg").innerHTML = "Cycle time must be an integer!";
    }
}

socket.on("updateSettings", function (msg) {
    objs = JSON.parse(msg);
    for (var i = 0; i < objs.length; ++i) {
        var itm = document.getElementById(objs[i].name);
        switch (itm.type) {
            case "text":
                itm.value = objs[i].value;
                break;
            case "checkbox":
                itm.checked = objs[i].value;
                break;
            case "radio":
                var radios = document.getElementsByName(objs[i].name);
                var ind = parseInt(objs[i].value);
                radios[ind].checked = true;
                break;
        }
    }
});
