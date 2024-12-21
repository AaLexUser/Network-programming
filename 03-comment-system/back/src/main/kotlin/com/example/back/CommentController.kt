package com.example.back

import org.springframework.http.HttpStatus
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.*
import java.util.*

@CrossOrigin
@RestController
class CommentController {
    private val comments = mutableListOf<CommentModel>()

    @GetMapping("/allComments")
    fun allComments(): List<CommentModel> {
        return comments.toList()
    }

    @PostMapping("/addComment")
    fun addComment(@RequestBody comment: CommentDto): ResponseEntity<Any> {
        if (comment.author.isEmpty() || comment.comment.isEmpty()) {
            return ResponseEntity(HttpStatus.BAD_REQUEST)
        }
        
        if (comment.author.length > 100 || comment.comment.length > 1000) {
            return ResponseEntity(HttpStatus.BAD_REQUEST)
        }

        val newComment = CommentModel(
            id = UUID.randomUUID(),
            author = comment.author,
            comment = comment.comment
        )
        comments.add(newComment)
        return ResponseEntity(newComment, HttpStatus.OK)
    }
}