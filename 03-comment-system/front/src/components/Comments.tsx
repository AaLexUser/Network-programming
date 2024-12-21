import React, { useEffect, useState } from "react";
import axios from "axios";
import { CommentModel } from "../model/CommentModel";
import { Box } from "@mui/material";
import { EMPTY_FIELD, MAX_LENGTH_100, MAX_LENGTH_1000 } from "../helper";
import { CommentList } from "./CommentList";
import { CommentForm } from "./CommentForm";

export const Comments = (): JSX.Element => {
    const [comments, setComments] = useState<CommentModel[]>([]);
    const [author, setAuthor] = useState("");
    const [comment, setComment] = useState("");
    const [authorError, setAuthorError] = useState("");
    const [commentError, setCommentError] = useState("");

    useEffect(() => {
        fetchComments();
        const interval = setInterval(fetchComments, 1000);
        return () => clearInterval(interval);
    }, []);

    const fetchComments = async () => {
        try {
            const { data } = await axios.get<CommentModel[]>("http://localhost:8080/allComments");
            setComments(data);
        } catch (error) {
            console.error("Error fetching comments:", error);
        }
    };

    const validateForm = (): boolean => {
        let isValid = true;
        
        if (!author) {
            setAuthorError(EMPTY_FIELD);
            isValid = false;
        } else if (author.length > 100) {
            setAuthorError(MAX_LENGTH_100);
            isValid = false;
        } else {
            setAuthorError("");
        }

        if (!comment) {
            setCommentError(EMPTY_FIELD);
            isValid = false;
        } else if (comment.length > 1000) {
            setCommentError(MAX_LENGTH_1000);
            isValid = false;
        } else {
            setCommentError("");
        }

        return isValid;
    };

    const handleSubmit = async () => {
        if (!validateForm()) return;

        try {
            const response = await axios.post("http://localhost:8080/addComment", {
                author,
                comment
            });

            if (response.status === 200) {
                setAuthor("");
                setComment("");
                await fetchComments();
            }
        } catch (error) {
            console.error("Error submitting comment:", error);
        }
    };

    const getAvatarColor = (author: string): string => {
        const hash = author.split('').reduce((acc, char) => acc + char.charCodeAt(0), 0);
        const hue = hash % 360; // 0-360 degrees for hue
        const saturation = 70; // Fixed saturation
        const lightness = 50; // Fixed lightness
        return `hsl(${hue}, ${saturation}%, ${lightness}%)`;
    };

    return (
        <Box sx={{ maxWidth: 800, margin: "0 auto", padding: 2, pb: 32 }}>
            <CommentList 
                comments={comments}
                getAvatarColor={getAvatarColor}
            />
            <CommentForm
                author={author}
                comment={comment}
                authorError={authorError}
                commentError={commentError}
                onAuthorChange={setAuthor}
                onCommentChange={setComment}
                onSubmit={handleSubmit}
            />
        </Box>
    );
};