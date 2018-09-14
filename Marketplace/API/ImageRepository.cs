using System;
using System.Linq;
using System.IO;

using Microsoft.EntityFrameworkCore;

using AutoMapper;

using MarktplaatsApi.Models;
using MarktplaatsApi.ViewModels;

namespace MarktplaatsApi.Services
{
    public class ImageRepository : CrudRepository<Image>, IDisposable
    {
        public static string ImageUploadPath;

        public ImageRepository(IMapper mapper) : base(mapper)
        {
        }

        public FileStream GetImageFile(Image image)
        {
            if(image == null)
            {
                return null;
            }

            FileStream imageFileStream;

            try
            {
                imageFileStream = File.OpenRead(ImageUploadPath + image.Id + "." + image.Extension);
            }
            catch
            {
                return null;
            }

            return imageFileStream;
        }

        public void DeleteImageFile(Image image)
        {
            File.Delete(ImageUploadPath + image.Id + "." + image.Extension);
        }

        public Image Edit(int id, ImageViewModel imageViewModel, string userId)
        {
            var image = FindById(imageViewModel.Id, x => x.Product.User);

            if (id != imageViewModel.Id || image == null || image.Product == null || 
                image.Product.User == null || image.Product.User.Id != userId)
            {
                return null;
            }

            image.OrderPosition = imageViewModel.OrderPosition;

            Edit(image);
            return image;
        }

        public Image Add(ImageViewModel imageViewModel, string userId)
        {
            var imageExtensionLower = imageViewModel.Extension.ToLower();

            // Check the file extension and size (jpg/png, 2 MB)
            if((imageExtensionLower != "jpg" && imageExtensionLower != "jpeg" &&
                imageExtensionLower != "png") ||
                imageViewModel.ImageData.Length > 2097152)
            {
                return null;
            }

            var product = context.Products.Include(x => x.Images).Include(x => x.User)
                .FirstOrDefault(c => c.Id == imageViewModel.ProductId);

            if(product == null || product.User == null || product.User.Id != userId ||
                (product.Images != null && product.Images.Count >= 6))
            {
                return null;
            }

            var image = mapper.Map<Image>(imageViewModel);
            image.Product = product;

            Add(image);

            if (image.Id != 0)
            {
                // Create the new image file and write all received image data to it
                var imagePath = ImageUploadPath + image.Id + "." + imageViewModel.Extension;
                File.Create(imagePath).Dispose();
                File.WriteAllBytes(imagePath, imageViewModel.ImageData);
            }

            return image;
        }

        public bool Remove(Image image, string userId)
        {
            if (image.Product == null || image.Product.User == null || 
                image.Product.User.Id != userId)
            {
                return false;
            }

            DeleteImageFile(image);
            Remove(image);

            return true;
        }
    }
}