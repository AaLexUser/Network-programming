import React from 'react';
import { Box, Button, Paper, TextField } from '@mui/material';

interface CommentFormProps {
    author: string;
    comment: string;
    authorError: string;
    commentError: string;
    onAuthorChange: (value: string) => void;
    onCommentChange: (value: string) => void;
    onSubmit: () => void;
}

export const CommentForm: React.FC<CommentFormProps> = ({
    author,
    comment,
    authorError,
    commentError,
    onAuthorChange,
    onCommentChange,
    onSubmit
}) => {
    return (
        <Box
            sx={{
                position: 'fixed',
                bottom: 0,
                left: 0,
                right: 0,
                bgcolor: 'background.default',
                p: 2,
                zIndex: 1000,
            }}
        >
            <Box sx={{ maxWidth: 800, margin: "0 auto" }}>
                <Paper sx={{ p: 2, mb: 2, borderRadius: 2 }}>
                    <Box component="form" sx={{ display: "flex", flexDirection: "column", gap: 3 }}>
                        <TextField
                            label="Author"
                            value={author}
                            onChange={(e) => onAuthorChange(e.target.value)}
                            error={!!authorError}
                            helperText={authorError}
                            inputProps={{ "data-testid": "author-field" }}
                            fullWidth
                            variant="outlined"
                        />
                        <TextField
                            label="Comment"
                            value={comment}
                            onChange={(e) => onCommentChange(e.target.value)}
                            error={!!commentError}
                            helperText={commentError}
                            multiline
                            minRows={3}
                            maxRows={5}
                            inputProps={{ "data-testid": "comment-field" }}
                            fullWidth
                            variant="outlined"
                        />
                    </Box>
                </Paper>

                <Paper sx={{ borderRadius: 2 }}>
                    <Button
                        variant="contained"
                        onClick={onSubmit}
                        fullWidth
                        sx={{ 
                            height: 56,
                            borderRadius: 2,
                            fontSize: '1rem'
                        }}
                        data-testid="submit-button"
                    >
                        SEND
                    </Button>
                </Paper>
            </Box>
        </Box>
    );
};
