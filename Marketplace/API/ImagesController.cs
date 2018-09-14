using System.Linq;
using System.Net;
using System.IO;

using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Authorization;

using AutoMapper;

using MarktplaatsApi.Services;
using MarktplaatsApi.Models;
using MarktplaatsApi.ViewModels;
using MarktplaatsApi.Authentication;
using Microsoft.EntityFrameworkCore;

namespace Marktplaats.Controllers
{
    [Authorize]
    [Route("api/[controller]")]
    public class ImagesController : Controller
    {
        private ImageRepository imageRepository;

        public ImagesController(IMapper mapper)
        {
            imageRepository = new ImageRepository(mapper);
        }

        // GET: api/Images/5/file
        [HttpGet("{id}/file")]
        [ProducesResponseType(typeof(File), (int)HttpStatusCode.OK)]
        public IActionResult GetImageFile(int id)
        {
            Image image;

            // If the given id is 0 get the default image, else get the requested product image
            if (id == 0)
            {
                image = new Image()
                {
                    Id = 0,
                    Extension = "jpg"
                };
            }
            else
            {
                image = imageRepository.FindById(id);
            }

            var imageFileStream = imageRepository.GetImageFile(image);

            if (imageFileStream == null)
            {
                return BadRequest();
            }

            return File(imageFileStream, "image/" + image.Extension);
        }

        // GET: api/Images/5
        [HttpGet("{id}", Name = "GetImage")]
        [ProducesResponseType(typeof(Image), (int)HttpStatusCode.OK)]
        public IActionResult GetImage(int id)
        {
            var image = imageRepository.FindById(id);

            if (image == null)
            {
                return NotFound();
            }

            return Ok(image);
        }

        // PUT: api/Images/5
        [HttpPut("{id}")]
        [ProducesResponseType(typeof(Image), (int)HttpStatusCode.OK)]
        public IActionResult PutImage(int id, [FromBody] ImageViewModel imageViewModel)
        {
            if (!ModelState.IsValid)
            {
                return BadRequest(ModelState);
            }

            var editedImage = imageRepository.Edit(id, imageViewModel, User.GetId());

            if (editedImage == null)
            {
                return BadRequest();
            }

            try
            {
                imageRepository.SaveChanges();
            }
            catch (DbUpdateConcurrencyException)
            {
                if (!ImageExists(id))
                {
                    return NotFound();
                }
                else
                {
                    throw;
                }
            }

            return Ok(editedImage);
        }

        // POST: api/Images
        [HttpPost]
        [ProducesResponseType(typeof(Image), (int)HttpStatusCode.OK)]
        public IActionResult PostImage([FromBody] ImageViewModel imageViewModel)
        {
            if (!ModelState.IsValid)
            {
                return BadRequest(ModelState);
            }

            var image = imageRepository.Add(imageViewModel, User.GetId());

            if(image == null)
            {
                return BadRequest();
            }

            return CreatedAtRoute("GetImage", new { id = image.Id }, image);
        }

        // DELETE: api/Images/5
        [HttpDelete("{id}")]
        [ProducesResponseType(typeof(Image), (int)HttpStatusCode.OK)]
        public IActionResult DeleteImage(int id)
        {
            var image = imageRepository.FindById(id, x => x.Product.User);

            if (image == null)
            {
                return NotFound();
            }

            var isRemoved = imageRepository.Remove(image, User.GetId());

            if(!isRemoved)
            {
                return BadRequest();
            }

            return Ok(image);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                imageRepository.Dispose();
            }
            base.Dispose(disposing);
        }

        private bool ImageExists(int id)
        {
            return imageRepository.GetAll().Count(e => e.Id == id) > 0;
        }
    }
}