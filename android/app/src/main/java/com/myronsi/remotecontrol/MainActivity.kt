package com.myronsi.remotecontrol

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.weight
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.consume
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                RemoteControlScreen()
            }
        }
    }
}

private class RemoteClient {
    private var socket: java.net.Socket? = null
    private var writer: java.io.BufferedWriter? = null

    @Synchronized
    fun connect(host: String, port: Int = 48721) {
        socket?.close()
        val newSocket = java.net.Socket(host, port)
        newSocket.tcpNoDelay = true
        socket = newSocket
        writer = newSocket.getOutputStream().bufferedWriter(Charsets.UTF_8)
        send("{\"version\":1,\"type\":\"hello\"}")
    }

    @Synchronized
    fun send(json: String) {
        val currentWriter = writer ?: return
        currentWriter.write(json)
        currentWriter.newLine()
        currentWriter.flush()
    }

    @Synchronized
    fun close() {
        writer = null
        socket?.close()
        socket = null
    }
}

@androidx.compose.runtime.Composable
private fun RemoteControlScreen() {
    val scope = rememberCoroutineScope()
    val client = remember { RemoteClient() }
    var host by remember { mutableStateOf("192.168.1.100") }
    var status by remember { mutableStateOf("Disconnected") }
    var connected by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Remote Control", style = MaterialTheme.typography.headlineMedium)
        Text(status)

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            OutlinedTextField(
                value = host,
                onValueChange = { host = it },
                modifier = Modifier.weight(1f),
                label = { Text("PC IP address") },
                singleLine = true,
                enabled = !connected
            )

            Button(
                onClick = {
                    if (connected) {
                        client.close()
                        connected = false
                        status = "Disconnected"
                    } else {
                        status = "Connecting..."
                        scope.launch(Dispatchers.IO) {
                            try {
                                client.connect(host.trim())
                                connected = true
                                status = "Connected"
                            } catch (e: Exception) {
                                connected = false
                                status = "Connection failed: ${e.message ?: "unknown error"}"
                            }
                        }
                    }
                }
            ) {
                Text(if (connected) "Disconnect" else "Connect")
            }
        }

        Spacer(Modifier.height(4.dp))

        Column(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .background(Color.LightGray)
                .pointerInput(connected) {
                    if (!connected) return@pointerInput
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        val dx = dragAmount.x.toInt()
                        val dy = dragAmount.y.toInt()
                        if (dx != 0 || dy != 0) {
                            scope.launch(Dispatchers.IO) {
                                client.send(
                                    "{\"version\":1,\"type\":\"mouse_move\",\"dx\":$dx,\"dy\":$dy}"
                                )
                            }
                        }
                    }
                },
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = androidx.compose.ui.Alignment.CenterHorizontally
        ) {
            Text("Touchpad", style = MaterialTheme.typography.headlineSmall)
            Text("Drag to move the PC cursor")
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Button(
                modifier = Modifier.weight(1f),
                enabled = connected,
                onClick = {
                    scope.launch(Dispatchers.IO) {
                        client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"left\",\"action\":\"down\"}")
                        client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"left\",\"action\":\"up\"}")
                    }
                }
            ) { Text("Left") }

            Button(
                modifier = Modifier.weight(1f),
                enabled = connected,
                onClick = {
                    scope.launch(Dispatchers.IO) {
                        client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"right\",\"action\":\"down\"}")
                        client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"right\",\"action\":\"up\"}")
                    }
                }
            ) { Text("Right") }
        }
    }
}
