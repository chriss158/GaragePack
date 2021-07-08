var uri = ""
var hostname = window.location.hostname;
if (hostname == "") {
    hostname = "192.168.12.175";
    uri = "http://" + hostname;
}

//on ready
$(function () {
    // ---------------- sockets -----------------
    var socketLog = new WebSocket('ws://' + hostname + '/logws');
    var socket = new WebSocket('ws://' + hostname + '/ws');
    getCurrenAttributes();

    socketLog.onmessage = function (evt) {
        console.log(evt.data);
    }

    socket.onmessage = function (evt) {
        try {
            var obj = JSON.parse(evt.data);
            parseAttributes(obj);
        }
        catch (e) {
            //console.log("Error parsing json data: " + evt.data );
        }
    }
    // ------------ UI --------------------
    $(".garage-status").click(function () {
        $(".garage-status").addClass("updating");
        if ($(this).hasClass("garage-closed") || $(this).hasClass("garage-undefined")) {
            $.get(uri + "/door?action=open");
        } else {
            $.get(uri + "/door?action=close");
        }
    });
    $(".nav-settings, #cancel").click(function () {
        $(".container").slideToggle();
    });
    $("#restart").click(function () {
        restartESP();
    });
    $(".nav-settings").click(function () {
        readSettings();
    });
    $("#save").click(function () {
        saveSettings();
    });
});

function restartESP() {
    $("body").css("cursor", "progress");
    $.post(uri + "/restart");
    setTimeout(function () {
        $("body").css("cursor", "default");
        window.location.reload();
    }, 2000);
}

function getCurrenAttributes() {
    $.ajax({
        dataType: "json",
        url: uri + "/attributes",
        success: function (data) {
            parseAttributes(data);
        }
    });
}

function parseAttributes(obj) {
    if (obj.motion !== undefined && obj.open !== undefined && obj.car !== undefined && obj.distance !== undefined) {
       
        $(".garage-status, .car").hide();
        $(".garage-status").removeClass("motion");
        if (obj.motion) {
            $(".garage-status").addClass("motion");
        }
        if (obj.garageState == "open" || obj.garageState == "closing") {
            $(".garage-open").show();
            if(obj.garageState == "open") $(".garage-status").removeClass("updating");
        } else if (obj.garageState == "closed"  || obj.garageState == "opening") {
            $(".garage-closed").show();
            if(obj.garageState == "closed") $(".garage-status").removeClass("updating");
        } else if (obj.garageState == "undefined") {
            $(".garage-undefined").show();
            $(".garage-status").removeClass("updating");
        }

        if (obj.car) {
            $(".car-present").show();
        } else {
            $(".car-notpresent").show();
        }

        //set placeholder
        $("#doorDistanceOpen, #doorDistanceClosed").prop("placeholder", obj.distance);
    }
    if (obj.wifiQuality !== undefined) {
        $(".wifi-quality").html(obj.wifiQuality + "%");
    }

}

function readSettings() {
    var jqxhr = $.get(uri + "/settings")
        .done(function (data) {
            // console.log(data);
            Object.keys(data).forEach(function (key) {
                if ($('#' + key).prop("type") == "checkbox") {
                    $('#' + key).prop("checked", data[key]);
                } else {
                    $('#' + key).val(data[key]);
                }
            });
        })
        .fail(function () {
            alert("Error reading settings");
        })
        ;
}

function saveSettings() {
    jdata = {};
    $(".setting input").each(function () {
        if (!this.reportValidity()) return;
        var val = $(this).val();
        if ($(this).prop("type") == "number") {
            val = parseInt(val);
        }
        if ($(this).prop("type") == "checkbox") {
            val = $(this).prop("checked");
        }
        jdata[$(this).prop("id")] = val;
    });

    var jqxhr = $.post(uri + "/settings", { body: JSON.stringify(jdata) })
        .done(function () {
            restartESP();
        })
        .fail(function () {
            alert("Error saving settings");
        })
        ;
}
