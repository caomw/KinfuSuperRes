function [ output_args ] = blend( I, J, alpha )
%BLEND Summary of this function goes here
%   Detailed explanation goes here

if ~exist( 'alpha', 'var' ),
    alpha = .5;
elseif isempty( alpha ),
    alpha = .5;
end

[ h1, w1, c1 ] = size(I);
[ h2, w2, c2 ] = size(J);
c3 = max( c1, c2 );

if ( (h1 ~= h2) || (w1 ~= w2) )
    disp('Warning, input in blend does need images to be the same SIZE!');
    return;
end


output = zeros( h1, w1, c3 );

for c = 1 : max(c1,c2)
    output( :, :, min(
    
end
q = p * alpha;
q(:,:,1) = q(:,:,1) + output * (1 - alpha);
q(:,:,2) = q(:,:,1) + output * (1 - alpha);
q(:,:,3) = q(:,:,1) + output * (1 - alpha);
imshow(q);


end

