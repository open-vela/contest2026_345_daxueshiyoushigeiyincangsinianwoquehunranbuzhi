#!/usr/bin/env python3
"""Shared image geometry and records for external TinyPose datasets."""
from __future__ import annotations

import hashlib
import math
import pathlib

import numpy as np
from PIL import Image, ImageDraw

from keypoint_dataset import KEYPOINTS, MOVENET_INDICES

COCO_KEYPOINTS = (
    'nose', 'left_eye', 'right_eye', 'left_ear', 'right_ear',
    'left_shoulder', 'right_shoulder', 'left_elbow', 'right_elbow',
    'left_wrist', 'right_wrist', 'left_hip', 'right_hip',
    'left_knee', 'right_knee', 'left_ankle', 'right_ankle',
)
SIDE = 192


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def frame_image(image: Image.Image, crop=None):
    """Integer crop + aspect-preserving letterbox; store exact realized scales."""
    width, height = image.size
    left, top, right, bottom = crop or (0, 0, width, height)
    left, top = max(0, math.floor(left)), max(0, math.floor(top))
    right, bottom = min(width, math.ceil(right)), min(height, math.ceil(bottom))
    if left >= right or top >= bottom:
        raise ValueError('empty image crop')
    cw, ch = right - left, bottom - top
    scale = SIDE / max(cw, ch)
    rw, rh = max(1, round(cw * scale)), max(1, round(ch * scale))
    px, py = (SIDE - rw) // 2, (SIDE - rh) // 2
    result = Image.new('RGB', (SIDE, SIDE), (0, 0, 0))
    result.paste(image.convert('RGB').crop((left, top, right, bottom)).resize(
        (rw, rh), Image.Resampling.BOX), (px, py))
    return result, {
        'source_width': width, 'source_height': height,
        'crop_xyxy': [left, top, right, bottom], 'resized_wh': [rw, rh],
        'pad_xy': [px, py], 'scale_xy': [rw / cw, rh / ch],
        'output_wh': [SIDE, SIDE], 'rotation': 'none',
        'coordinate_convention': 'x,y in image extents normalized by 192',
    }


def coco_points(raw, transform):
    if len(raw) != 51:
        raise ValueError('expected 17 COCO keypoint triples')
    left, top, right, bottom = transform['crop_xyxy']
    sx, sy = transform['scale_xy']
    px, py = transform['pad_xy']
    points = []
    for index, name in enumerate(COCO_KEYPOINTS):
        x, y, visibility = raw[index * 3:index * 3 + 3]
        if visibility not in (0, 1, 2) or not math.isfinite(x + y):
            raise ValueError('invalid COCO coordinates/visibility')
        in_frame = visibility > 0 and left <= x < right and top <= y < bottom
        usable = visibility == 2 and in_frame
        points.append({
            'name': name,
            'x': min(1.0, max(0.0, ((x - left) * sx + px) / SIDE))
                 if visibility else 0.5,
            'y': min(1.0, max(0.0, ((y - top) * sy + py) / SIDE))
                 if visibility else 0.5,
            'confidence': float(usable), 'usable': usable,
            'visibility': ('visible' if usable else 'outside_crop' if visibility and not in_frame
                           else 'occluded' if visibility == 1 else 'unlabeled'),
            'coco_visibility': visibility,
        })
    return [dict(points[i]) for i in MOVENET_INDICES], points


def save_frame(image, output, name):
    path = output / name
    image.save(path, format='PPM')
    return {'image': name, 'width': SIDE, 'height': SIDE, 'sha256': sha256(path)}


def image_quality(image):
    rgb = np.asarray(image, dtype=np.float32)
    gray = rgb.mean(axis=2)
    return {'mean_brightness': float(gray.mean()),
            'dark_pixel_fraction': float(np.mean(gray < 20)),
            'bright_pixel_fraction': float(np.mean(gray > 235)),
            'gradient_energy': float(np.mean(np.diff(gray, axis=0) ** 2) +
                                     np.mean(np.diff(gray, axis=1) ** 2))}


def review_sheet(records, directory, output, limit=48, start_index=None):
    """Contact sheet: draw only usable bones, retain image IDs for correction."""
    rows = records[:limit]
    if not rows:
        return
    sheet = Image.new('RGB', (SIDE * 6, (SIDE + 24) * math.ceil(len(rows) / 6)))
    edges = ((0, 1), (0, 2), (1, 3), (2, 3), (2, 4), (3, 5), (4, 6), (5, 7))
    for index, record in enumerate(rows):
        with Image.open(directory / record['image']) as source:
            tile = source.convert('RGB')
        draw = ImageDraw.Draw(tile)
        points = record.get('keypoints', [])
        for a, b in edges:
            if points and points[a]['usable'] and points[b]['usable']:
                draw.line([(points[i]['x'] * SIDE, points[i]['y'] * SIDE)
                           for i in (a, b)], fill=(255, 255, 0), width=2)
        for point in points:
            if point['usable']:
                x, y = point['x'] * SIDE, point['y'] * SIDE
                draw.ellipse((x-2, y-2, x+2, y+2), fill=(255, 40, 40))
        ox, oy = (index % 6) * SIDE, (index // 6) * (SIDE + 24)
        sheet.paste(tile, (ox, oy))
        caption = record['image']
        if start_index is not None:
            caption = f'{start_index + index:03d} {caption}'
        ImageDraw.Draw(sheet).text((ox + 2, oy + SIDE + 3), caption, fill='white')
    sheet.save(output)


def validate_record(record, directory):
    if [p['name'] for p in record['keypoints']] != list(KEYPOINTS):
        raise ValueError('8-point order mismatch')
    for point in record['keypoints']:
        if any(not math.isfinite(point[k]) or not 0 <= point[k] <= 1
               for k in ('x', 'y', 'confidence')):
            raise ValueError('nonfinite or out-of-range normalized point')
    if sha256(directory / record['image']) != record['sha256']:
        raise ValueError('image hash mismatch')
    with Image.open(directory / record['image']) as image:
        if image.size != (SIDE, SIDE) or image.mode != 'RGB':
            raise ValueError('image must be RGB192')
