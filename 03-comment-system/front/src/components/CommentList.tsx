import React, { useRef, useEffect } from 'react';
import { List } from '@mui/material';
import { CommentModel } from '../model/CommentModel';
import { CommentItem } from './CommentItem';

interface CommentListProps {
    comments: CommentModel[];
    getAvatarColor: (author: string) => string;
}

export const CommentList: React.FC<CommentListProps> = ({ comments, getAvatarColor }) => {
    const listRef = useRef<HTMLDivElement>(null);
    const prevCommentsLength = useRef(comments.length);

    useEffect(() => {
        if (comments.length > prevCommentsLength.current) {
            if (listRef.current && typeof listRef.current.scrollIntoView === 'function') {
                listRef.current.scrollIntoView({ behavior: 'smooth', block: 'start' });
            }
        }
        prevCommentsLength.current = comments.length;
    }, [comments]);

    return (
        <List sx={{ mb: 10, borderRadius: 5}}>
            {comments.map((comment) => (
                <CommentItem 
                    key={comment.id}
                    comment={comment}
                    getAvatarColor={getAvatarColor}
                />
            ))}
            <div ref={listRef} />
        </List>
    );
};
