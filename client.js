// import WebSocket from 'ws';
const WebSocket = require("ws").WebSocket;

ws = new WebSocket("ws://10.0.0.105:1235/")

ws.onopen = () => {
  console.log("Connection opened")
  ws.send("Hi server, please send me the score of yesterday's game")
}

ws.onmessage = (event) => {
  console.log("Data received", event.data)
  ws.close() // We got the score so we don't need the connection anymore
}

ws.onclose = (event) => {
  console.log("Connection closed", event.code, event.reason, event.wasClean)
}

ws.onerror = () => {
  console.log("Connection closed due to error")
}
