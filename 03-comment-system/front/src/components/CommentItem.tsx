import React from 'react';
import { Avatar, Box, ListItem, Typography } from '@mui/material';
import { CommentModel } from '../model/CommentModel';

interface CommentItemProps {
    comment: CommentModel;
    getAvatarColor: (author: string) => string;
}

export const CommentItem: React.FC<CommentItemProps> = ({ comment, getAvatarColor }) => {
    return (
        <ListItem 
            sx={{ 
                display: 'flex', 
                alignItems: 'flex-start',
                gap: 2,
                mb: 2
            }}
        >
            <Avatar 
                sx={{ 
                    bgcolor: getAvatarColor(comment.author),
                    width: 40,
                    height: 40
                }}
            >
                {comment.author[0].toUpperCase()}
            </Avatar>
            <Box>
                <Typography variant="subtitle1" fontWeight="bold" sx={{ mb: 0.5 }}>
                    {comment.author}
                </Typography>
                <Typography variant="body1" sx={{ color: 'text.secondary' }}>
                    {comment.comment}
                </Typography>
            </Box>
        </ListItem>
    );
};
