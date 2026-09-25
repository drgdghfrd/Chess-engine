package com.chesszero.app

import android.app.Activity
import android.os.Bundle
import android.text.InputType
import android.view.Gravity
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future

private const val START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

class MainActivity : Activity() {
    private lateinit var engine: NativeChessZero
    private lateinit var board: ChessBoardView
    private lateinit var fenInput: EditText
    private lateinit var depthInput: EditText
    private lateinit var timeInput: EditText
    private lateinit var threadsInput: EditText
    private lateinit var hashInput: EditText
    private lateinit var status: TextView
    private lateinit var detail: TextView
    private lateinit var searchButton: Button
    private lateinit var stopButton: Button
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private var searchFuture: Future<*>? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        engine = NativeChessZero()

        val netOk = engine.nnueLoaded()
        engine.setThreads(2)
        engine.setHashMb(64)
        engine.setPosition(START_FEN)

        board = ChessBoardView(this).apply {
            setFen(START_FEN)
            setOnMoveListener { uci -> playHumanMove(uci) }
        }
        status = TextView(this).apply {
            text = if (netOk) "ChessZero 1.0.9 • NNUE embedded • White: you • Black: engine" else "ChessZero 1.0.9 • NNUE load failed"
            textSize = 18f
            setPadding(0, 12, 0, 12)
        }
        detail = TextView(this).apply {
            text = "Ready"
            setPadding(0, 8, 0, 16)
        }

        searchButton = Button(this).apply {
            text = "Engine move"
            setOnClickListener { searchEngineMove() }
        }
        stopButton = Button(this).apply {
            text = "Stop"
            isEnabled = false
            setOnClickListener { engine.stop() }
        }
        val newGameButton = Button(this).apply {
            text = "New game"
            setOnClickListener { newGame() }
        }
        val undoButton = Button(this).apply {
            text = "Undo turn"
            setOnClickListener { undoTurn() }
        }
        val actionRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            addView(newGameButton, LinearLayout.LayoutParams(0, -2, 1f))
            addView(undoButton, LinearLayout.LayoutParams(0, -2, 1f))
            addView(searchButton, LinearLayout.LayoutParams(0, -2, 1f))
            addView(stopButton, LinearLayout.LayoutParams(0, -2, 1f))
        }

        fenInput = EditText(this).apply {
            setText(START_FEN)
            hint = "FEN for analysis/setup"
            minLines = 2
        }
        val loadFenButton = Button(this).apply {
            text = "Load FEN"
            setOnClickListener { loadFen() }
        }
        depthInput = numberField("8", "Depth")
        timeInput = numberField("1000", "Move time (ms), 0 = depth only")
        threadsInput = numberField("2", "Threads (1–8)")
        hashInput = numberField("64", "Hash MB")

        val controls = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(TextView(this@MainActivity).apply {
                text = "Analysis controls"
                textSize = 16f
                setPadding(0, 20, 0, 6)
            })
            addView(depthInput)
            addView(timeInput)
            addView(threadsInput)
            addView(hashInput)
            addView(fenInput)
            addView(loadFenButton)
        }

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(20, 20, 20, 24)
            addView(status)
            addView(board, LinearLayout.LayoutParams(-1, dp(320)))
            addView(actionRow)
            addView(detail)
            addView(controls)
        }
        val scroll = ScrollView(this).apply { addView(content) }
        setContentView(scroll)
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun numberField(value: String, hint: String): EditText = EditText(this).apply {
        setText(value)
        this.hint = hint
        inputType = InputType.TYPE_CLASS_NUMBER
    }

    private fun playHumanMove(uci: String) {
        if (engine.isSearching()) return
        val fen = engine.currentFen()
        if (!fen.split(" ")[1].equals("w")) {
            detail.text = "Wait for the engine move."
            return
        }
        if (!engine.makeMove(uci)) {
            detail.text = "Illegal move: $uci"
            board.setFen(engine.currentFen())
            return
        }
        refreshBoard("You played $uci")
        searchEngineMove()
    }

    private fun searchEngineMove() {
        if (engine.isSearching()) return
        val fen = engine.currentFen()
        if (!fen.split(" ")[1].equals("b")) {
            detail.text = "Engine moves as Black."
            return
        }
        val depth = depthInput.text.toString().toIntOrNull()?.coerceIn(1, 64) ?: 8
        val movetime = timeInput.text.toString().toIntOrNull()?.coerceAtLeast(0) ?: 1000
        val threads = threadsInput.text.toString().toIntOrNull()?.coerceIn(1, 8) ?: 2
        val hash = hashInput.text.toString().toIntOrNull()?.coerceIn(1, 512) ?: 64
        if (!engine.setThreads(threads) || !engine.setHashMb(hash)) {
            detail.text = "Configuration error: ${engine.lastError()}"
            return
        }
        searchButton.isEnabled = false
        stopButton.isEnabled = true
        detail.text = "ChessZero is thinking…"
        searchFuture = executor.submit {
            val raw = engine.search(depth, movetime)
            val result = parseSearchResult(raw)
            runOnUiThread {
                stopButton.isEnabled = false
                searchButton.isEnabled = true
                if (result != null && result.bestMove.length >= 4) {
                    if (engine.makeMove(result.bestMove)) {
                        refreshBoard("Engine: ${result.bestMove} • depth ${result.depth} • ${result.nodes} nodes\nPV ${result.pv}")
                    } else {
                        detail.text = "Engine move could not be applied: ${result.bestMove}"
                    }
                } else {
                    detail.text = raw
                }
                searchFuture = null
            }
        }
    }

    private fun newGame() {
        if (engine.isSearching()) return
        if (engine.newGame()) {
            board.setFen(START_FEN)
            fenInput.setText(START_FEN)
            detail.text = "New game — your move."
        }
    }

    private fun undoTurn() {
        if (engine.isSearching()) return
        executor.submit {
            engine.undoMove()
            engine.undoMove()
            val fen = engine.currentFen()
            runOnUiThread {
                board.setFen(fen)
                fenInput.setText(fen)
                detail.text = "Turn undone."
            }
        }
    }

    private fun loadFen() {
        if (engine.isSearching()) return
        val fen = fenInput.text.toString().trim()
        if (engine.setPosition(fen)) {
            board.setFen(fen)
            detail.text = "FEN loaded."
        } else {
            detail.text = "FEN error: ${engine.lastError()}"
        }
    }

    private fun refreshBoard(message: String) {
        val fen = engine.currentFen()
        board.setFen(fen)
        fenInput.setText(fen)
        detail.text = message
    }

    override fun onDestroy() {
        searchFuture?.cancel(false)
        engine.stop()
        executor.submit { engine.waitIdle() }
        executor.shutdown()
        try {
            executor.awaitTermination(2, java.util.concurrent.TimeUnit.SECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
        engine.close()
        super.onDestroy()
    }
}
