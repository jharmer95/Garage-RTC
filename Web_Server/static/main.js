// Create socket
var socket = io.connect("http://" + document.domain + ":" + location.port);

// On socket event "connect", emit "my event" with JSON object
socket.on("connect", function () {
  socket.emit("my event", {
    data: "User Connected"
  });

  // on form submission, capture username and message and emit a "my event" with the data in JSON
  var form = $("form").on("submit", function (e) {
    e.preventDefault();
    let user_name = $("input.username").val();
    let user_input = $("input.message").val();
    socket.emit("my event", {
      user_name: user_name,
      message: user_input
    });

    // Set focus on input.message
    $("input.message")
      .val("")
      .focus();
  });
});

// On socket event "my response" (from python), remove "No message" and append the chat message
socket.on("my response", function (msg) {
  console.log(msg);
  if (typeof msg.user_name !== "undefined") {
    $("h3").remove();
    $("div.message_holder").append(
      '<div><b style="color: #000">' +
      msg.user_name +
      "</b> " +
      msg.message +
      "</div>"
    );
  }
});
