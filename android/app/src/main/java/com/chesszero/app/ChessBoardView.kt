package com.chesszero.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

class ChessBoardView(context: Context) : View(context) {
    private val light = Paint(Paint.ANTI_ALIAS_FLAG)
    private val dark = Paint(Paint.ANTI_ALIAS_FLAG)
    private val piecePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        isSubpixelText = true
    }
    private val selectionPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 6f
    }
    private val lastMovePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        alpha = 72
    }
    private val pieceSizeFactor = 0.72f
    private var board = CharArray(64) { ' ' }
    private var whiteToMove = true
    private var selected = -1
    private var dragX = Float.NaN
    private var dragY = Float.NaN
    private var lastFrom = -1
    private var lastTo = -1
    private var moveListener: ((String) -> Unit)? = null

    init {
        isClickable = true
        contentDescription = "Chess board"
    }

    fun setOnMoveListener(listener: (String) -> Unit) {
        moveListener = listener
    }

    fun setFen(fen: String) {
        val parts = fen.trim().split("\\s+".toRegex())
        if (parts.isEmpty()) return
        val rows = parts[0].split('/')
        if (rows.size != 8) return
        val next = CharArray(64) { ' ' }
        for (row in 0 until 8) {
            var file = 0
            for (ch in rows[row]) {
                if (ch.isDigit()) file += ch.digitToInt()
                else if (file < 8) next[(7 - row) * 8 + file++] = ch
            }
        }
        board = next
        whiteToMove = parts.getOrNull(1) != "b"
        selected = -1
        dragX = Float.NaN
        dragY = Float.NaN
        lastFrom = -1
        lastTo = -1
        invalidate()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)
        val side = when {
            width > 0 && height > 0 -> min(width, height)
            width > 0 -> width
            else -> height
        }
        setMeasuredDimension(side, side)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val side = width.toFloat().coerceAtMost(height.toFloat())
        val square = side / 8f
        light.color = 0xfff0d9b5.toInt()
        dark.color = 0xffb58863.toInt()
        lastMovePaint.color = 0xffcdd26a.toInt()
        selectionPaint.color = 0xff5f7cff.toInt()

        for (rank in 0 until 8) {
            for (file in 0 until 8) {
                val x = file * square
                val y = rank * square
                canvas.drawRect(x, y, x + square, y + square, if ((rank + file) and 1 == 0) light else dark)
                val index = (7 - rank) * 8 + file
                if (index == lastFrom || index == lastTo) {
                    canvas.drawRect(x, y, x + square, y + square, lastMovePaint)
                }
            }
        }

        if (selected >= 0) {
            val rank = 7 - selected / 8
            val file = selected % 8
            val rect = RectF(file * square + 3, rank * square + 3,
                (file + 1) * square - 3, (rank + 1) * square - 3)
            canvas.drawRoundRect(rect, 8f, 8f, selectionPaint)
        }

        piecePaint.textSize = square * pieceSizeFactor
        val fm = piecePaint.fontMetrics
        val baselineOffset = -(fm.ascent + fm.descent) / 2f
        for (rank in 0 until 8) {
            for (file in 0 until 8) {
                val index = (7 - rank) * 8 + file
                val piece = board[index]
                if (piece == ' ') continue
                val glyph = glyph(piece)
                val x = file * square + square / 2f
                val y = rank * square + square / 2f + baselineOffset
                piecePaint.color = if (piece.isUpperCase()) 0xffffffff.toInt() else 0xff202124.toInt()
                piecePaint.setShadowLayer(3f, 1f, 1f, 0x66000000)
                canvas.drawText(glyph, x, y, piecePaint)
                piecePaint.clearShadowLayer()
            }
        }

        if (!dragX.isNaN() && selected >= 0) {
            val piece = board[selected]
            piecePaint.textSize = square * pieceSizeFactor
            piecePaint.color = if (piece.isUpperCase()) 0xffffffff.toInt() else 0xff202124.toInt()
            piecePaint.setShadowLayer(3f, 1f, 1f, 0x66000000)
            val glyph = glyph(piece)
            canvas.drawText(glyph, dragX, dragY - (piecePaint.fontMetrics.ascent + piecePaint.fontMetrics.descent) / 2f, piecePaint)
            piecePaint.clearShadowLayer()
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val square = width.toFloat() / 8f
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                val sq = hit(event.x, event.y, square) ?: return true
                if (board[sq] != ' ' && board[sq].isUpperCase() == whiteToMove) {
                    selected = sq
                    dragX = event.x
                    dragY = event.y
                    invalidate()
                }
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (selected >= 0) {
                    dragX = event.x
                    dragY = event.y
                    invalidate()
                }
                return true
            }
            MotionEvent.ACTION_UP -> {
                val from = selected
                val to = hit(event.x, event.y, square)
                dragX = Float.NaN
                dragY = Float.NaN
                if (from >= 0 && to != null && from != to) {
                    val uci = squareName(from) + squareName(to) + promotionSuffix(from, to)
                    moveListener?.invoke(uci)
                }
                selected = -1
                invalidate()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                selected = -1
                dragX = Float.NaN
                dragY = Float.NaN
                invalidate()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun hit(x: Float, y: Float, square: Float): Int? {
        if (x < 0 || y < 0 || x >= square * 8 || y >= square * 8) return null
        val file = (x / square).toInt().coerceIn(0, 7)
        val row = (y / square).toInt().coerceIn(0, 7)
        return (7 - row) * 8 + file
    }

    private fun squareName(index: Int): String {
        val file = 'a' + (index % 8)
        val rank = '1' + (index / 8)
        return "$file$rank"
    }

    private fun promotionSuffix(from: Int, to: Int): String {
        val piece = board[from]
        if (piece.lowercaseChar() != 'p') return ""
        val rank = to / 8
        return if (rank == 0 || rank == 7) "q" else ""
    }

    private fun glyph(piece: Char): String = when (piece) {
        'K' -> "♔"; 'Q' -> "♕"; 'R' -> "♖"; 'B' -> "♗"; 'N' -> "♘"; 'P' -> "♙"
        'k' -> "♚"; 'q' -> "♛"; 'r' -> "♜"; 'b' -> "♝"; 'n' -> "♞"; 'p' -> "♟"
        else -> ""
    }
}
