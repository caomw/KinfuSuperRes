% example: detail enhancement
% figure 6 in our paper

close all;

I = im2double( imread('mapped.png'       ) );
p = im2double(imread('img8_00000015.png'));

r = 16;
eps = 0.1^2;
output = guidedFilter( I(:, :), rgb2gray(p), r, eps );

alpha = .1;
figure();
q = p * alpha;
q(:,:,1) = q(:,:,1) + output * (1 - alpha);
q(:,:,2) = q(:,:,1) + output * (1 - alpha);
q(:,:,3) = q(:,:,1) + output * (1 - alpha);
imshow(q);


%q(:, :, 2) = guidedFilter(I(:, :, 2), p(:, :, 2), r, eps);
%q(:, :, 3) = guidedFilter(I(:, :, 3), p(:, :, 3), r, eps);

I_enhanced = (I - output) * 5 + output;

figure();
imshow([I, q, I_enhanced], [0, 1]);
